namespace vtconsole {
// Unbind all active vtconsole from frame buffer.
// Automatically registers an exit handler to restore to previous state.
void unbind_all(bool restore_on_exit = false);
}
