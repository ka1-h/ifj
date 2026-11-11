// Syntakticky test: prikazy v bloku (val/var, priradenie, strednik, return, vnoreny blok).
// Platny IFJ26 kod. Vyrazy su trivialne (identifikator, literal, volanie).
// Pravidla: 23-35
// Ocakavany vysledok: kod 0.
import ifj26.*
def f(a: Int): Int =
    val x = 1
    val y: Int = 2
    var z: String = "s"
    var w: Boolean = true
    z = "t"
    x = y; y = x; z = "u"
    val blk =
        val inner = 1
        inner
    g(x, y)
    x
def unitRet(): Unit =
    return
def valRet(): Int =
    return 1
def single(): Int = a
def literals() =
    val d = 1.5
    val e = 1e3
    val u = ()
    val b = false
    1
