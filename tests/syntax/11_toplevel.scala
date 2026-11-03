// Syntakticky test: definicie na najvyssej urovni (type, case class, def, @main).
// Platny IFJ26 kod.
// Pravidla: 4, 5, 6, 7, 8, 9, 11, 12, 13-17, 18, 19
// Ocakavany vysledok: kod 0.
import ifj26.*
type Num = Int | Double
type Anything = Int | Double | String | Boolean | Unit | Point
type Fn = (Int, String) => Boolean
type Thunk = () => Unit
type Higher = ((Int) => Int, Int) => (Int) => Int
case class Point(x: Int, y: Int)
case class Empty()
case class WithFn(f: (Int) => Int, g: () => Unit)
def noParams: Int = 1
def noType() = 1
def multi(a: Int)(b: Int, c: String)(): Int = a
def unionParam(x: Int | String): Int | String = x
def fnParam(f: (Int) => Int, g: () => Unit): Unit = g
@main
def main(): Unit =
    return
