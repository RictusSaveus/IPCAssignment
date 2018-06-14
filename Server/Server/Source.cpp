#include <Windows.h>

#include <type_traits>

#include "json.hpp"

using nlohmann::json;

namespace {

	struct point {
		float x;
		float y;

		float get_x() const {
			return x;
		}

		float get_y() const {
			return y;
		}

		void move(float by_x, float by_y) {
			x += by_x;
			y += by_y;
		}

		// Complex math :D
		float slow_add() {
			Sleep(2000);
			float sum = 0.0f;
			sum += x;
			sum += y;
			return sum;
		}

		static void service_request(const HANDLE pipe, const json& request) {
			assert(pipe != INVALID_HANDLE_VALUE);
			if (request["call"] == "get_x") {
				::std::vector<uint8_t> data = request["orig_data"];
				point point_ = *reinterpret_cast<point*>(data.data());
				float x = point_.get_x();
				WriteFile(pipe, &x, sizeof(float), NULL, NULL);
			}
			else if (request["call"] == "get_y") {
				::std::vector<uint8_t> data = request["orig_data"];
				point point_ = *reinterpret_cast<point*>(data.data());
				float y = point_.get_y();
				WriteFile(pipe, &y, sizeof(float), NULL, NULL);
			}
			else if (request["call"] == "move") {
				::std::vector<uint8_t> data = request["orig_data"];
				point point_ = *reinterpret_cast<point*>(data.data());
				float x = request["x"];
				float y = request["y"];
				point_.move(x, y);
				WriteFile(pipe, &point_, sizeof(point), NULL, NULL);
			}
			else if (request["call"] == "slow_add") {
				::std::vector<uint8_t> data = request["orig_data"];
				point point_ = *reinterpret_cast<point*>(data.data());
				float sum = point_.slow_add();
				WriteFile(pipe, &sum, sizeof(float), NULL, NULL);
			}
		}

	};

	// Defined as template function to allow extensibility
	template <typename T>
	json get_meta() {}

	template <> json get_meta<point>() {
		json meta;
		meta["name"] = "point";
		meta["size"] = sizeof(point);
		meta["functions"].push_back({ { "name", "get_x" },{ "out", "float" } , {"fun_type", "sync"} });
		meta["functions"].push_back({ { "name", "get_y" },{ "out", "float" } ,{ "fun_type", "sync" } });
		meta["functions"].push_back({ { "name", "move" },{ "args",{ { { "name", "x" },{ "type", "float" } },
			{ { "name", "y" },{ "type", "float" } } } }, { "fun_type", "sync" } });
		meta["functions"].push_back({ {"name", "slow_add"}, { "out", "float" }, { "fun_type", "async" } });
		return meta;
	}

	// Resizable buffer
	::std::vector<uint8_t> buffer;

	class NamedPipe {
	public:
		const HANDLE pipe;

		NamedPipe() = delete;

		// Throw if pipe is not created.
		NamedPipe(const char* name) : pipe(CreateNamedPipe(name, 
			PIPE_ACCESS_DUPLEX | 
			FILE_FLAG_OVERLAPPED, 
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 
			1, 1024, 1024, 0, NULL)) {
			if (pipe == INVALID_HANDLE_VALUE) {
				throw ::std::exception("Pipe could not be created");
			}
		}

		// Explicitly deleting move/copy assignment op/ctors.

		NamedPipe(const NamedPipe&) = delete;
		NamedPipe& operator=(const NamedPipe&) = delete;

		NamedPipe(NamedPipe&&) = delete;
		NamedPipe& operator=(NamedPipe&&) = delete;

		// Main loop of server
		void handle_connection() {
			if (ConnectNamedPipe(pipe, NULL) != FALSE) {
				DWORD bytes_read;
				buffer.resize(1024);
				while (ReadFile(pipe, buffer.data(), 1024, &bytes_read, NULL) != FALSE){
					buffer.resize(bytes_read);
					json command = nlohmann::json::from_ubjson(buffer);
					if (command["request"] == "meta") {
						json meta = get_meta<point>();
						auto data = json::to_ubjson(meta);
						WriteFile(pipe, data.data(), data.size(), NULL, NULL);
					}

					if (command["request"] == "creation") {
						if (command["type"] == "point") {
							point new_point{ 0.0f, 0.0f };
							WriteFile(pipe, &new_point, sizeof(point), NULL, NULL);
						}
					}

					if (command["request"] == "function") {
						if (command["type"] == "point") {
							point::service_request(pipe, command);
						}
					}

					if (command["request"] == "close") {
						DisconnectNamedPipe(pipe);
						break;
					}


					buffer.resize(1024);
				}
			}
		}

	};
}

int main() {
	

	NamedPipe npipe("\\\\.\\pipe\\Pipe");

	while (true) {
		npipe.handle_connection();
	}
	return 0;
}