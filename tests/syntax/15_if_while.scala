// Syntakticky test: if/else (inline aj blokove, else if, if ako hodnota) a while.
// Platny IFJ26 kod. Podmienky su trivialne vyrazy.
// Pravidla: 35, 36, 37, 38, 46.1
// Ocakavany vysledok: kod 0.
import ifj26.*
def f(a: Int): Int =
    if a then b
    if a then b else c
    if a then
        b
    if a then
        b
    else
        c
    if a then
        b
    else if c then
        d
    else
        e
    if a then return 1 else return 2
    if a then -1 else !b
    val v = if a then 1 else 2
    val w = if a then 1 else
        2
    while a do b = c
    while a do
        b = c
        if b then
            while c do
                d
    while a do
        b
    a
