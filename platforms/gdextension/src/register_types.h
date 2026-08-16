#ifndef REGISTER_TYPES_H
#define REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void initialize_godot_terminal_module(ModuleInitializationLevel p_level);
void uninitialize_godot_terminal_module(ModuleInitializationLevel p_level);

} // namespace godot

#endif // REGISTER_TYPES_H
