// Syntakticky test bez vyrazov: @main, zoznamy parametrov, prikazy na riadku.
// Platny IFJ26 kod.
// Pravidla: 4.1, 5.1, 6, 7, 8-10, 12.1, 24-28, 29.2, 30.1, 34, 35.2
// Ocakavany vysledok: kod 0. Kym chyba EOL za DEDENT: kod 2, parser zastavi na T_EOF na konci suboru.

import ifj26.*
@main
def main(a: Int, b: String,)(f: (Int, String) => Boolean)()(g: () => Unit): Unit =
  return
  return; return; return
