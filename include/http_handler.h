#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <string>
#include <map>
#include <functional>
#include <microhttpd.h>
#include "api_request.h"
#include "dynamic_router.h"

class HttpHandler {
public:
    HttpHandler();
    ~HttpHandler();

    // Request handling
    enum MHD_Result handleRequest(struct MHD_Connection* connection,
                     const char* url, const char* method,
                     const char* version, const char* upload_data,
                     size_t* upload_data_size, void** con_cls);

    // Route handlers (new structured approach)
    void addStructuredRouteHandler(const std::string& path, RouteProcessor processor);
    
    // Legacy route handlers (for backward compatibility)
    void addRouteHandler(const std::string& path, 
                        std::function<std::string(const std::string& method, 
                                                  const std::map<std::string, std::string>& params,
                                                  const std::string& body)> handler);
    
    // Dynamic route handlers (for pattern-based routing)
    void addDynamicRoute(const std::string& pattern,
                        std::function<std::string(const std::string& method, 
                                                  const std::map<std::string, std::string>& params,
                                                  const std::string& body)> handler);

private:
    // New structured route processors
    std::map<std::string, RouteProcessor> structured_route_processors_;
    
    // Legacy route handlers
    std::map<std::string, std::function<std::string(const std::string&, 
                                                   const std::map<std::string, std::string>&,
                                                   const std::string&)>> route_handlers_;
    
    // Dynamic router for pattern-based routing
    std::unique_ptr<DynamicRouter> dynamic_router_;

    // Request processing (structured approach)
    ApiRequest buildApiRequest(struct MHD_Connection* connection, const char* url, 
                              const char* method, const char* upload_data, 
                              size_t* upload_data_size, void** con_cls);
    enum MHD_Result sendApiResponse(struct MHD_Connection* connection, const ApiResponse& response);
    
    // Legacy request processing
    std::map<std::string, std::string> parseQueryString(const std::string& query);
    std::map<std::string, std::string> parseHeaders(struct MHD_Connection* connection);
    std::string getRequestBody(const char* upload_data, size_t upload_data_size);
    
    // Response helpers
    enum MHD_Result sendResponse(struct MHD_Connection* connection, 
                    int status_code, 
                    const std::string& content,
                    const std::string& content_type = "text/html");
    
    enum MHD_Result sendJsonResponse(struct MHD_Connection* connection,
                        int status_code,
                        const std::string& json_content);
    
    enum MHD_Result sendErrorResponse(struct MHD_Connection* connection,
                         int status_code,
                         const std::string& error_message);
};

#endif // HTTP_HANDLER_H