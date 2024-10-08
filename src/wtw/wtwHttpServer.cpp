#include "wtwhttpserver.h"
#include "utils/config-data.h"
#include "plugin-support.h"
#include "translation-service/translation.h"

#include <httplib.h>
#include <obs-module.h>
#include <thread>
#include <nlohmann/json.hpp>

// start the http server
void start_wtw_http_server()
{
	obs_log(LOG_INFO, "Starting Polyglot WTW http server thread...");

	std::thread([]() {
		// create the server
		if (global_context.wtwsvr != nullptr) {
			obs_log(LOG_INFO, "Polyglot WTW Http server already running, stopping...");
			stop_wtw_http_server();
		}
		global_context.wtwsvr = new httplib::Server();

		global_context.wtwsvr->set_pre_routing_handler([](const httplib::Request &req,
							       httplib::Response &res) {
			res.set_header("Access-Control-Allow-Origin", "*");
			res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
			res.set_header("Access-Control-Allow-Credentials", "true");
			
			res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");

			std::string infoFormat = "POLYGLOT WTW - Received request EN PREREQUEST: " + req.path + " || " + req.method;
			obs_log(LOG_INFO, infoFormat.c_str() );

			// Si la requête est une requête préliminaire OPTIONS
			if (req.method == "OPTIONS") {
				// Répondre avec un statut 204 No Content
				res.status = 204;
				return httplib::Server::HandlerResponse::Handled;
			}

			return httplib::Server::HandlerResponse::Unhandled;
		});

		/*global_context.wtwsvr->Options(R"(\*)", [](const auto& req, auto& res) {
			res.set_header("Allow", "GET, POST, HEAD, OPTIONS");
		});

		global_context.wtwsvr->Options("/translate", [](const auto& req, auto& res) {
			res.set_header("Access-Control-Allow-Origin", req.get_header_value("Origin").c_str());
			res.set_header("Allow", "GET, POST, HEAD, OPTIONS");
			res.set_header("Access-Control-Allow-Headers", "X-Requested-With, Content-Type, Accept, Origin, Authorization");
			res.set_header("Access-Control-Allow-Methods", "OPTIONS, GET, POST, HEAD");
		});*/

		// set an echo handler
		global_context.wtwsvr->Post("/echo", [](const httplib::Request &req,
						     httplib::Response &res,
						     const httplib::ContentReader &content_reader) {
			UNUSED_PARAMETER(req);
			obs_log(LOG_DEBUG, "Received request on /echo");
			std::string body;
			content_reader([&](const char *data, size_t data_length) {
				body.append(data, data_length);
				return true;
			});

			//Avoid CORS Error
			//res.set_header("Access-Control-Allow-Origin", "*");

			res.set_content(body, "text/plain");
			res.status = 200;
		});
		// set a translation handler
		global_context.wtwsvr->Post(
			"/translate", [](const httplib::Request &req, httplib::Response &res,
					 const httplib::ContentReader &content_reader) {
				UNUSED_PARAMETER(req);
				obs_log(LOG_DEBUG, "Received request on /translate");
				std::string body;
				content_reader([&](const char *data, size_t data_length) {
					body.append(data, data_length);
					return true;
				});

				//Avoid CORS Error
				//res.set_header("Access-Control-Allow-Origin", "*");

				std::string result;
				int ret = translate_from_json(body, result);
				if (ret == OBS_POLYGLOT_TRANSLATION_SUCCESS) {
					res.set_content(result, "text/plain");
					res.status = 200;
				} else {
					res.set_content("Translation failed", "text/plain");
					res.status = 500;
				}
			});

		// listen on the port
		obs_log(LOG_INFO, "Polyglot Http server starting on port %d",
			global_config.wtw_http_server_port);
		try {
			global_context.wtwsvr->listen("127.0.0.1", global_config.wtw_http_server_port);
		} catch (const std::exception &e) {
			obs_log(LOG_ERROR, "Polyglot Http WTW server start error: %s", e.what());
		}
		obs_log(LOG_INFO, "Polyglot Http WTW server stopped.");
	}).detach();

	global_context.status_callback("Ready for requests at http://localhost:" +
				       std::to_string(global_config.wtw_http_server_port) +
				       "/translate");
}

// stop the http server
void stop_wtw_http_server()
{
	obs_log(LOG_INFO, "Stopping Polyglot WTW http server...");
	if (global_context.wtwsvr == nullptr) {
		obs_log(LOG_INFO, "Polyglot WTW Http server not running.");
	} else {
		global_context.wtwsvr->stop();
		delete global_context.wtwsvr;
		global_context.wtwsvr = nullptr;
		global_context.status_callback("");
	}
}
