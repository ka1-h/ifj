import ifj26.*
@main def start(): Unit =
    for i <- 3 to 1 by -1 do print(i)
    for
        j <- 0 until 5
        if j != 1
        if j < 4
    do
        print(j)
