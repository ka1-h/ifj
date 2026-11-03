// Syntakticky test: vetvy case nie su odsadene za match.
// Neplatny IFJ26 kod.
// Ocakavany vysledok: kod 2.
import ifj26.*
def f(x: Int) =
    x match
    case _ => 1
