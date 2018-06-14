#include <vector>
#include <iostream>
#include <set>
#include <cassert>

#include "json.hpp"

#include <Windows.h>

using nlohmann::json;


namespace {

	// Resizable buffer
	::std::vector<uint8_t> buffer;

	json build_function_string(const json& function) {
		json command;
		command["request"] = "function";
		command["call"] = function["name"];
		if (function.find("args") != ::std::end(function)) {
			for (auto& arg : function["args"]) {
				::std::cout << "Enter value for " << arg["name"].get<::std::string>() <<"\n";
				if (arg["type"] == "float") {
					float in;
					::std::cin >> in;
					::std::string arg_name = arg["name"];
					command[arg_name] = in;
				}
				else if (arg["type"] == "int") {
					int in;
					::std::cin >> in;
					::std::string arg_name = arg["name"];
					command[arg_name] = in;
				}
				else if (arg["type"] == "double") {
					double in;
					::std::cin >> in;
					::std::string arg_name = arg["name"];
					command[arg_name] = in;
				}
			}
		}

		return command;
	}

	// Wrapper to hold Server class information
	struct ClassInfo {
		size_t size;
		::std::string name;
		json functions;

		friend bool operator<(const ClassInfo& c1, const ClassInfo& c2) {
			return c1.name < c2.name;
		}
	};

	// Wrapper to hold Object data
	struct ClientObject {
		::std::string object_name;
		::std::vector<uint8_t> data;
	};

	void print(uint8_t* buffer, const ::std::string& type) {
		if (type == "float") {
			::std::cout << *reinterpret_cast<float*>(buffer);
		}
		else if (type == "double") {
			::std::cout << *reinterpret_cast<double*>(buffer);
		}
		else if (type == "int") {
			::std::cout << *reinterpret_cast<int*>(buffer);
		}
		::std::cout << "\n";
	}

	class PipeConnection {
		::std::set<ClassInfo> types;

		::std::map<ClassInfo, ::std::vector<ClientObject>> objects;

		::std::tuple<bool, OVERLAPPED, ::std::string> overlapped_info;
	public:
		const HANDLE pipe;

		PipeConnection() = delete;

		PipeConnection(const char* name) : pipe(CreateFile(name,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL)){
			if (pipe == INVALID_HANDLE_VALUE) {
				throw ::std::exception("Pipe Connection could not be established");
			}

			json command;
			command["request"] = "meta";
			
			auto json_bytes = nlohmann::json::to_ubjson(command);

			if (!WriteFile(pipe, json_bytes.data(), sizeof(uint8_t) * json_bytes.size(), NULL, NULL)) {
				throw ::std::exception("Meta data could not be fetched.");
			}

			DWORD bytes_in_pipe;
			buffer.resize(1024);
			if (!ReadFile(pipe, buffer.data(), 1024, &bytes_in_pipe, NULL)) {
				throw ::std::exception("Meta data could not be fetched.");
			}

			buffer.resize(bytes_in_pipe);
			json meta = nlohmann::json::from_ubjson(buffer);

			ClassInfo info;
			info.name = meta["name"].get<::std::string>();
			info.functions = meta["functions"];
			info.size = meta["size"];

			types.insert(info);

			::std::get<0>(overlapped_info) = true;
		}

		// Explicitly deleting move/copy assignment op/ctors.

		PipeConnection(PipeConnection&) = delete;
		PipeConnection& operator=(const PipeConnection&) = delete;

		PipeConnection(PipeConnection&&) = delete;
		PipeConnection& operator=(PipeConnection&&) = delete;


		// Main loop of client
		bool run() {
			buffer.resize(1024);

			if (!::std::get<0>(overlapped_info)) {
				if (HasOverlappedIoCompleted(&::std::get<1>(overlapped_info))) {
					DWORD bytes_read;
					ReadFile(pipe, buffer.data(), 1024, NULL, &::std::get<1>(overlapped_info));
					::std::cout << "Async function result is\n";
					print(buffer.data(), ::std::get<2>(overlapped_info));
					::std::get<0>(overlapped_info) = true;
				}
			}

			::std::cout << 
				"1. Get available types\n" <<
				"2. Get existing objects\n" <<
				"3. Create new object" << "\n" <<
				"4. Operate on existing object" << "\n" <<
				"5. Close Connection\n";
			int choice;
			::std::cin >> choice;
			switch (choice)
			{
			case 1: {
				::std::cout << "Available types are :\n";
				for (auto& type : types) {
					::std::cout << type.name << "\n";
				}
				return true;
			}
			case 2: {
				::std::cout << "Available objects are\n";

				for (auto& pair : objects) {
					for (auto& obj : pair.second) {
						::std::cout << obj.object_name << "\n";
					}
				}
				
				return true;
			}
			case 3: {
				if (!::std::get<0>(overlapped_info)) {
					::std::cout << "Async operation in progress. Pipe inaccessible!\n";
					return true;
				}

				::std::cout << "Available types are :\n";
				for (auto& type : types) {
					::std::cout << type.name << "\n";
				}

				::std::cout << "Enter desired type\n";
				::std::string requested_type;
				::std::cin >> requested_type;

				ClassInfo key;
				key.name = requested_type;
				auto it_type = types.find(key);

				if (it_type == ::std::end(types)) {
					return true;
				}

				::std::cout << "Enter name of new object\n";
				::std::string obj_name;
				::std::cin >> obj_name;

				json command;
				command["request"] = "creation";
				command["type"] = requested_type;
				auto request_bytes = nlohmann::json::to_ubjson(command);

				if (WriteFile(pipe, request_bytes.data(), request_bytes.size(), NULL, NULL)) {

					ClientObject obj;
					obj.data.resize(it_type->size);
					obj.object_name = obj_name;

					if (ReadFile(pipe, obj.data.data(), it_type->size, NULL, NULL)) {
						objects[key].push_back(::std::move(obj));
						::std::cout << "Object created\n";
					}
				}
				return true;
			}

			case 4: {
				if (!::std::get<0>(overlapped_info)) {
					::std::cout << "Async operation in progress. Pipe inaccessible!\n";
					return true;
				}

				::std::cout << "Types of object to be operated on\n";
				for (auto& type : types) {
					::std::cout << type.name << "\n";
				}

				::std::cout << "Enter type\n";
				::std::string requested_type;
				::std::cin >> requested_type;

				ClassInfo key;
				key.name = requested_type;
				if (objects[key].empty()) {
					return true;
				}

				::std::cout << "Available objects are\n";

				for (auto& obj : objects[key]) {
					::std::cout << obj.object_name << "\n";
				}

				::std::cout << "Enter object name to operate on\n";
				::std::string object_name;
				::std::cin >> object_name;

				auto it_obj = ::std::find_if(::std::begin(objects[key]), ::std::end(objects[key]),
					[object_name](const ClientObject& obj) {return obj.object_name == object_name; });

				if (it_obj == ::std::end(objects[key])) {
					return true;
				}

				::std::cout << "Available functions are\n";
				auto it_set = types.find(key);
				for (auto& fun : it_set->functions) {
					::std::cout << fun["name"].get<::std::string>() <<"\n";
				}

				::std::cout << "Enter function name\n";
				::std::string function_name;
				::std::cin >> function_name;

				auto it_fun = ::std::find_if(::std::begin(it_set->functions), ::std::end(it_set->functions),
					[function_name](const json& j) {return j["name"] == function_name; });
				json command = build_function_string(*it_fun);
				command["type"] = requested_type;
				command["orig_data"] = it_obj->data;

				auto json_bytes = json::to_ubjson(command);
				if ((*it_fun)["fun_type"] == "async") {
					assert(it_fun->find("out") != ::std::end(*it_fun));
					::std::get<0>(overlapped_info) = false;
					::std::get<1>(overlapped_info) = {};
					::std::get<2>(overlapped_info) = (*it_fun)["out"].get<::std::string>();
					if (!WriteFile(pipe, json_bytes.data(), json_bytes.size(), NULL, NULL)) {
						// Reset and return;
						::std::get<0>(overlapped_info) = true;
						return true;
					}
					DWORD bytes_read;
					if (ReadFile(pipe, buffer.data(), 1024, &bytes_read, &::std::get<1>(overlapped_info))) {
						::std::cout << "Async function result is\n";
						print(buffer.data(), ::std::get<2>(overlapped_info));
						::std::get<0>(overlapped_info) = true;
					}
				}
				else if(WriteFile(pipe, json_bytes.data(), json_bytes.size(), NULL, NULL)) {
					DWORD bytes_in_pipe;
					if (ReadFile(pipe, buffer.data(), 1024, &bytes_in_pipe, NULL)) {
						buffer.resize(bytes_in_pipe);
						if (it_fun->find("out") != ::std::end(*it_fun)) {
							print(buffer.data(), (*it_fun)["out"]);
						}
						else {
							it_obj->data = buffer;
							::std::cout << "Object moved.\n";
						}
					}
				}

				return true;;
			}
			case 5: {
				if (!::std::get<0>(overlapped_info)) {
					::std::cout << "Async operation in progress. Cannot close!\n";
					return true;
				}
				json command;
				command["request"] = "close";

				auto json_bytes = json::to_ubjson(command);
				WriteFile(pipe, json_bytes.data(), json_bytes.size(), NULL, NULL);
				return false;
			}
			default:
				return true;
			}
		}

		~PipeConnection() {
			CloseHandle(pipe);
		}
	};
}

int main(void)
{
	PipeConnection pconn("\\\\.\\pipe\\Pipe");
	while (pconn.run()){
	}

	return (0);
}