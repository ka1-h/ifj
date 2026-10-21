// Syntakticky test bez vyrazov: extension a funkcia bez zoznamu parametrov.
// Platny IFJ26 kod.
// Pravidla: 4.5, 6, 7.2, 20-22
// Ocakavany vysledok: kod 0. Kym chyba EOL za DEDENT: kod 2, parser zastavi na druhom T_DEDENT na konci suboru.

import ifj26.*
extension (value: Int)
  def twice: Int =
    return
