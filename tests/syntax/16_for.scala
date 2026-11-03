// Syntakticky test: for (jednoriadkova a viacriadkova hlavicka, to/until/by, straze).
// Platny IFJ26 kod. Meze a straze su trivialne vyrazy.
// Pravidla: 39-45
// Ocakavany vysledok: kod 0.
import ifj26.*
def f(n: Int): Unit =
    for i <- 1 to 10 do
        print(i)
    for i <- 0 until n do print(i)
    for i <- 1 to n by 2 do
        x
    for i <- 1 to n if i if j do
        x
    for
        i <- 0 until n
        if i
        if j
    do
        x
    for
        i <- 0 to n by 1 if a
    do x
    for i <- a to b do
        for j <- c until d do
            x
    return
