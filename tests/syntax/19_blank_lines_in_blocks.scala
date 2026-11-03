// Syntakticky test: prazdne riadky, riadky s medzerami a komentare vnutri blokov.
// Platny IFJ26 kod (spec 1.2: take riadky sa ignoruju a nemenia odsadenie).
// Pravidla: 23-27, 36
// Ocakavany vysledok: kod 0.
import ifj26.*
def f(a: Int) =
    val x = 1

    val y = 2
  
    // komentar s rovnakym odsadenim
// komentar s mensim odsadenim
        // komentar s vacsim odsadenim
    if a then

        b

    else
        c


    x

