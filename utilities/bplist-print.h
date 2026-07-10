#pragma once

// Utility -- give it a string of bytes containing a binary plist
// and an indent depth.
// Warning: not proof against malformed data!

int pretty_print_binary_plist(const char *buf, size_t size, int depth);