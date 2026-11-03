// Syntakticky test: subor konci vo vnorenom bloku bez znaku LF na konci.
// Platny IFJ26 kod (spec 1.2: riadok je ukonceny LF alebo EOF).
// Pravidla: 23, 27.2, 73, 74
// Ocakavany vysledok: kod 0.
import ifj26.*
def f(a: Int) =
    while a do
        if a then
            b