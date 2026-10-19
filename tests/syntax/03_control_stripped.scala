// Syntakticky test: if, while, for, val/var, priradenie.
// POZOR: vyrazy su VYMAZANE, nie je to platny IFJ26 kod. Povodny tvar je v komentari
// na konci riadku. Ked bude hotova precedencna SA, vyrazy treba doplnit spat.
// Pravidla: 29-33, 35.1, 36-47
// Ocakavany vysledok: kod 0. Kym chyba EOL za DEDENT: kod 2, parser zastavi na druhom T_DEDENT na konci suboru.

import ifj26.*
def f(): Unit =
  if then return  // if a then return
  if then return else return  // if a then return else return
  if then return else if then return else return  // if a then return else if b then return else return
  while do return  // while a do return
  for i <- to do return  // for i <- 0 to 10 do return
  for i <- until by do return  // for i <- 0 until 10 by 2 do return
  for i <- to by if if do return  // for i <- 0 to 10 by 2 if a if b do return
  for
    i <- to by
    if if
    if
  do return
  val v = if then return else return  // val v = if a then return else return
  var w: Int = if then return else return  // var w: Int = if a then return else return
  w = if then return else return  // w = if a then return else return
  return if then return else return  // return if a then return else return
  if then
    return
  else
    return
