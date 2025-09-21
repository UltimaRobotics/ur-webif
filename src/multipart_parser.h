
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

struct MultipartFile {
    std::string field_name;
    std::string filename;
    std::string content_type;
    std::vector<uint8_t> data;
    size_t size;
};

struct MultipartField {
    std::string field_name;
    std::string value;
};

class MultipartParser {
public:
    MultipartParser(const std::string& content_type);
    ~MultipartParser();

    bool parse(const std::string& body);
    
    const std::vector<std::shared_ptr<MultipartFile>>& getFiles() const { return files_; }
    const std::vector<std::shared_ptr<MultipartField>>& getFields() const { return fields_; }
    
    std::shared_ptr<MultipartFile> getFile(const std::string& field_name) const;
    std::string getField(const std::string& field_name) const;
    
    bool hasFile(const std::string& field_name) const;
    bool hasField(const std::string& field_name) const;

private:
    std::string boundary_;
    std::vector<std::shared_ptr<MultipartFile>> files_;
    std::vector<std::shared_ptr<MultipartField>> fields_;
    
    bool extractBoundary(const std::string& content_type);
    bool parseMultipartData(const std::string& body);
    std::string extractHeaderValue(const std::string& headers, const std::string& name);
    std::map<std::string, std::string> parseContentDisposition(const std::string& value);
};
