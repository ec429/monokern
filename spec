monokern: a project to create a kerned monospace font for terminals and code.

"Wait, did you say /kerned monospace/?"  Yes, I did - and that's not as ridiculous as it sounds.  The alignment of columnar and tabular text that monospace provides relies not on the spacing itself being constant, but of the sum of the spacing over longer runs being constant.  Thus, so long as a narrow pair-kerning is balanced out by a wide pair-kerning nearby, tables will remain aligned.
The basic idea behind monokern is /deviation kerning/: any individual character is permitted to deviate a fixed amount from its grid position.  In the initial experiments, this will be 1px, because we're working with smallish fonts around 10 or 12pt.  Then, since interword spacing can be crushed or expanded a couple of px without the sky falling (hence why justified text is readable), the software need only optimise deviation across each word.
So, if you have a word of length n, the space of possible deviations has size 3^n, which you /could/ brute-force if you had to (for a moderately long word n=10, 3^n=59,049).  However, for very long words this could be troublesome - consider that for n=14, 3^n=4,782,969 which is starting to get a bit too big.  So, we need to find a decent algorithm for optimising this; I think dynamic programming techniques could solve it.
One approach here is to split the word in half and solve each half for each 'endpoint deviation'; for instance if the word is "moderately", split it into "moder" and "ately", kern "moder" subject to each of the constraints "the 'r' deviates by -1", "the 'r' deviates by 0" and "the 'r' deviates by 1"; similarly for ("ately", 'a'); this takes 6.3^4=486.  Now you have 9 cases to consider; just add the scores for each half to the score for the kerning of 'r' and 'a' that results, and pick the best one.
For still longer words we can recurse on this; in general if we constrain both endpoints of a 2n-letter word, we have a search space of 9^(n-1), but if we split it into two n-letter words, and solve each separately for each 'middle endpoint', we have 2.3^(n-1)+9 ops, which for large n is clearly smaller than 9^(n-1) since 3^n is o(9^n).
n	2.3^(n-1)+9	9^(n-1)
1	11			1
2	15			9
3	27			81
So if n≥3, we should divide and conquer.
In the case of an odd-length word 2n+1, there is also a solution: constrain on the deviation of the middle letter.  Then instead of 3^(2n-1) we have 2.3^(n-1)+3 ops.
n	2.3^(n-1)+3	3^(2n-1)
1	5			3
2	9			27
So if n≥2, we should divide and conquer.
Conclusion: choosing the method depending on whether n is even or odd, we should split an n-letter word whenever n≥5.
Note also that for longer words, it might be more efficient still to split into more than two parts immediately (eg. for n=9 it might be better to split into three 3s than a 4, a 2 and a 3 (which the above produces), but this would increase the implementation complexity of the algorithm, and I hate that sort of thing.

Complexity analysis:
n		cost
1		0 (special case)
2		3 (special case)
3		27
4		81
5		2c(3)+3 = 57
6		6c(3)+9 = 171
7		2c(4)+3 = 165
8		6c(4)+9 = 495
9		2c(5)+3 = 117
10		6c(5)+9 = 351
11		2c(6)+3 = 345
12		6c(6)+9 = 1035
13		2c(7)+3 = 333
14		6c(7)+9 = 999
15		2c(8)+3 = 993
16		6c(8)+9 = 2979

worst case is for n=2^k
c(2^k)=6c(2^(k-1))+9=O(6^k)=O(n^(log2(6))~=O(n^2.585) which is good enough

Things to do:
1. Implement the above algorithm taking as input a table of pair scores (or class-pair scores?)
2. Create a table of pair scores for some suitable monospace font (eg. the xterm default VT font)
3. Implement a routine to draw text using the results of (1) and (2) above.
4. Either proselytise terminal developers, or write a simple terminal emulator from scratch, or fork an existing terminal emulator.
