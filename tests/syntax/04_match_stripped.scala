// Syntakticky test: match a vzory.
// POZOR: vyrazy su VYMAZANE, nie je to platny IFJ26 kod. Povodny tvar je v komentari
// na konci riadku. Ked bude hotova precedencna SA, vyrazy treba doplnit spat.
// Kazda vetva ma ako hodnotu "if then return else return", lebo ina hodnota bez vyrazu nejde.
// Pravidla: 47.2, 48-59
// Ocakavany vysledok: kod 0. Kym chyba EOL za DEDENT: kod 2, parser zastavi na druhom T_DEDENT na konci suboru.

import ifj26.*
def f(): Unit =
  return match  // return x match
    case _ => if then return else return
    case n: Int => if then return else return
    case n: Int | Double if => if then return else return  // case n: Int | Double if n > 0 => ...
    case Point(x, _, 1, -2, -3.5, "s", true, false, ()) => if then return else return
    case Point() => if then return else return
    case 0 => if then return else return
    case 1.5 => if then return else return
    case "text" => if then return else return
    case true => if then return else return
    case false => if then return else return
    case -1 => if then return else return
    case () => if then return else return
