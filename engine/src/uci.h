// UCI (Universal Chess Interface): the standard text protocol chess GUIs and tools use to
// talk to engines. Implementing it lets python-chess (and Arena, CuteChess, etc.) drive our
// engine as a subprocess — no language bindings needed. This is how our C++ core and the
// Python side communicate.
#pragma once

void uci_loop();   // read commands from stdin, reply on stdout, until "quit"
