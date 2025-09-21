
#include "multipart_parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>

MultipartParser::MultipartParser(const std::string& content_type) {
    if (!extractBoundary(content_type)) {
        std::cout << "[MULTIPART] Failed to extract boundary from content type" << std::endl;
    }
}

MultipartParser::~MultipartParser() {
    files_.clear();
    fields_.clear();
}

bool MultipartParser::parse(const std::string& body) {
    if (boundary_.empty()) {
        std::cout << "[MULTIPART] No boundary found, cannot parse" << std::endl;
        return false;
    }
    
    return parseMultipartData(body);
}

std::shared_ptr<MultipartFile> MultipartParser::getFile(const std::string& field_name) const {
    for (const auto& file : files_) {
        if (file->field_name == field_name) {
            return file;
        }
    }
    return nullptr;
}

std::string MultipartParser::getField(const std::string& field_name) const {
    for (const auto& field : fields_) {
        if (field->field_name == field_name) {
            return field->value;
        }
    }
    return "";
}

bool MultipartParser::hasFile(const std::string& field_name) const {
    return getFile(field_name) != nullptr;
}

bool MultipartParser::hasField(const std::string& field_name) const {
    for (const auto& field : fields_) {
        if (field->field_name == field_name) {
            return true;
        }
    }
    return false;
}

bool MultipartParser::extractBoundary(const std::string& content_type) {
    const std::string boundary_prefix = "boundary=";
    size_t pos = content_type.find(boundary_prefix);
    
    if (pos == std::string::npos) {
        return false;
    }
    
    pos += boundary_prefix.length();
    size_t end_pos = content_type.find(';', pos);
    
    if (end_pos == std::string::npos) {
        boundary_ = content_type.substr(pos);
    } else {
        boundary_ = content_type.substr(pos, end_pos - pos);
    }
    
    // Remove quotes if present
    if (boundary_.length() >= 2 && boundary_[0] == '"' && boundary_.back() == '"') {
        boundary_ = boundary_.substr(1, boundary_.length() - 2);
    }
    
    std::cout << "[MULTIPART] Extracted boundary: " << boundary_ << std::endl;
    return !boundary_.empty();
}

bool MultipartParser::parseMultipartData(const std::string& body) {
    const std::string delimiter = "--" + boundary_;
    const std::string end_delimiter = "--" + boundary_ + "--";
    
    size_t pos = 0;
    
    // Find first boundary
    pos = body.find(delimiter, pos);
    if (pos == std::string::npos) {
        std::cout << "[MULTIPART] No starting boundary found" << std::endl;
        return false;
    }
    
    pos += delimiter.length();
    
    while (pos < body.length()) {
        // Skip CRLF after boundary
        if (pos < body.length() && body[pos] == '\r') pos++;
        if (pos < body.length() && body[pos] == '\n') pos++;
        
        // Find next boundary or end
        size_t next_boundary = body.find(delimiter, pos);
        if (next_boundary == std::string::npos) {
            break;
        }
        
        // Extract part data
        std::string part = body.substr(pos, next_boundary - pos);
        
        // Remove trailing CRLF
        while (!part.empty() && (part.back() == '\r' || part.back() == '\n')) {
            part.pop_back();
        }
        
        // Split headers and data
        size_t headers_end = part.find("\r\n\r\n");
        if (headers_end == std::string::npos) {
            headers_end = part.find("\n\n");
            if (headers_end == std::string::npos) {
                pos = next_boundary + delimiter.length();
                continue;
            }
            headers_end += 2;
        } else {
            headers_end += 4;
        }
        
        std::string headers = part.substr(0, headers_end);
        std::string data = part.substr(headers_end);
        
        // Parse Content-Disposition header
        std::string content_disposition = extractHeaderValue(headers, "Content-Disposition");
        auto disposition_params = parseContentDisposition(content_disposition);
        
        std::string field_name = disposition_params["name"];
        std::string filename = disposition_params["filename"];
        
        if (!filename.empty()) {
            // File field
            auto file = std::make_shared<MultipartFile>();
            file->field_name = field_name;
            file->filename = filename;
            file->content_type = extractHeaderValue(headers, "Content-Type");
            file->data.assign(data.begin(), data.end());
            file->size = file->data.size();
            
            files_.push_back(file);
            std::cout << "[MULTIPART] Parsed file: " << filename << " (" << file->size << " bytes)" << std::endl;
        } else {
            // Regular field
            auto field = std::make_shared<MultipartField>();
            field->field_name = field_name;
            field->value = data;
            
            fields_.push_back(field);
            std::cout << "[MULTIPART] Parsed field: " << field_name << " = " << data.substr(0, 50) << std::endl;
        }
        
        pos = next_boundary + delimiter.length();
    }
    
    std::cout << "[MULTIPART] Parsing completed: " << files_.size() << " files, " << fields_.size() << " fields" << std::endl;
    return true;
}

std::string MultipartParser::extractHeaderValue(const std::string& headers, const std::string& name) {
    std::string search_name = name + ":";
    size_t pos = headers.find(search_name);
    
    if (pos == std::string::npos) {
        return "";
    }
    
    pos += search_name.length();
    
    // Skip whitespace
    while (pos < headers.length() && (headers[pos] == ' ' || headers[pos] == '\t')) {
        pos++;
    }
    
    size_t end_pos = headers.find('\r', pos);
    if (end_pos == std::string::npos) {
        end_pos = headers.find('\n', pos);
    }
    
    if (end_pos == std::string::npos) {
        return headers.substr(pos);
    }
    
    return headers.substr(pos, end_pos - pos);
}

std::map<std::string, std::string> MultipartParser::parseContentDisposition(const std::string& value) {
    std::map<std::string, std::string> params;
    
    std::istringstream iss(value);
    std::string token;
    
    // Skip the disposition type (e.g., "form-data")
    std::getline(iss, token, ';');
    
    while (std::getline(iss, token, ';')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        size_t eq_pos = token.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = token.substr(0, eq_pos);
            std::string val = token.substr(eq_pos + 1);
            
            // Remove quotes
            if (val.length() >= 2 && val[0] == '"' && val.back() == '"') {
                val = val.substr(1, val.length() - 2);
            }
            
            params[key] = val;
        }
    }
    
    return params;
}
