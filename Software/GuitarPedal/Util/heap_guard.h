// The heap's size and a guard against running out of it.
//
// newlib's malloc takes its memory from _sbrk, which here grows from `end` (right after the static data in RAM_D2, see nam_a2_sections.lds) with no limit: past the end of
// RAM_D2 (0x30048000) there is no memory, and the first access is a bus fault or a hang that looks like a lockup. heap_guard.cpp replaces _sbrk with one that stops at the end
// of RAM_D2 and then shows what happened on the display (and halts), instead. The functions here tell how much heap is left; the "Show Debug" screen prints it.
#pragma once
#ifndef HEAP_GUARD_H
#define HEAP_GUARD_H

#include <cstddef>

namespace heapguard {

/** How far the heap has grown in bytes. The break only moves up (freed memory is reused, not given back), so this is also the most the heap has ever needed. */
size_t Used();
/** What is left before the end of RAM_D2, in bytes. */
size_t Free();

/** "heap 123K free 123K" (used, then free): at most 21 characters for figures up to 999K, which is all that fits the 128 pixels of the display in the 6 pixel font. */
void Summary(char *buffer, size_t size);

/** Draws three lines of text on the display; set by the application once the display works. */
using HaltHandler = void (*)(const char *line1, const char *line2, const char *line3);
void SetHaltHandler(HaltHandler handler);

/** What the firmware is doing (for example "starting Mars"), named in the message if the heap runs out meanwhile. Must stay valid (a literal or something that lives on). */
void SetStage(const char *what);

/** Shows the three lines through the handler and stops. */
[[noreturn]] void Halt(const char *line1, const char *line2, const char *line3);

} // namespace heapguard

#endif
