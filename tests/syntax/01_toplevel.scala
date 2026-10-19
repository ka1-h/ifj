// Syntakticky test bez vyrazov: definicie na najvyssej urovni bez blokov.
// Platny IFJ26 kod.
// Pravidla: 1, 2, 3, 4.3, 4.4, 8-11, 13-19, 73
// Ocakavany vysledok: kod 0.

import ifj26.*
type Num = Int | Double
type Anything = Int | Double | String | Boolean | Unit | Shape
type Fn = (Int, String) => Boolean
type Thunk = () => Unit
type Higher = ((Int) => Int, Int) => (Int) => Int
case class Point(x: Int, y: Int)
case class Named(name: String,)
case class Empty()
case class WithFn(f: (Int) => Int, g: () => Unit)
