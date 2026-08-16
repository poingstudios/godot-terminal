#include "register_types.h"

#include "terminal_pty.h"
#include "terminal_emulator.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace godot {

void initialize_godot_terminal_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_class<TerminalPty>();
	ClassDB::register_class<TerminalEmulator>();
}

void uninitialize_godot_terminal_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

} // namespace godot

extern "C" {

GDExtensionBool GDE_EXPORT godot_terminal_library_init(
	GDExtensionInterfaceGetProcAddress p_get_proc_address,
	const GDExtensionClassLibraryPtr p_library,
	GDExtensionInitialization *r_initialization
) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(godot::initialize_godot_terminal_module);
	init_obj.register_terminator(godot::uninitialize_godot_terminal_module);
	init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}

}
