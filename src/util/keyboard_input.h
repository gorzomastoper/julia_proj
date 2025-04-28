#pragma once

#include "types.h"
#include <minwindef.h>
#include <windows.h>
#include <mutex>
#include <thread>

class keyboard_input {
	private:
		DWORD 				fwd_old_mode;
		HANDLE 				hStdin;
		std::thread			thread;
		WPARAM				event_queue[64];
		u8					event_count;
		std::mutex			mut;
	public:
		keyboard_input () {
			event_count = 0;
			hStdin = GetStdHandle(STD_INPUT_HANDLE);
			GetConsoleMode(hStdin, &fwd_old_mode);
			SetConsoleMode(hStdin, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);
			
			thread = std::thread([&](){
				while(true) {
					DWORD stub;
					INPUT_RECORD 	input_record[64];
					ReadConsoleInput(hStdin, input_record, 64 - event_count, &stub);
					mut.lock();
					for(u8 i = 0; i < 64 - event_count; ++i) {
						switch(input_record[i].EventType) {
							case KEY_EVENT: {
								event_queue[event_count] = input_record[i].Event.KeyEvent;
								++event_count;
							}; break;
						}
					}
					mut.unlock();
				}
			});
		}
	
		template<typename F>
		void handle_events(F f) {
			mut.lock();
			for(u8 i = 0; i < event_count; ++i) {
				f(event_queue[i]);
			}
			event_count = 0;
			mut.unlock();
		}
	
		void destroy() {
			SetConsoleMode(hStdin, fwd_old_mode);
			thread.~thread();
		}
	};