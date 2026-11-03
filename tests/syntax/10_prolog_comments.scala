// Syntakticky test: komentare a prazdne riadky pred prologom a medzi definiciami.
// Platny IFJ26 kod.
// Pravidla: 1, 2, 3, 72
// Ocakavany vysledok: kod 0.

/* blokovy
   komentar */

import ifj26.*


// komentar medzi definiciami
def a() = 1
   
/* komentar */
def b() = 2 // komentar za kodom
