# EDA-Game
A bot playing the game Ork Island.
 * More information can be found at [Jutge](https://jutge.org/problems/P35895_en)
 
 ## How to play the game?
 
1. Open a console and cd to the directory where you extracted the source code. 

2. Run `make all` to build the game and all the players. Note that Makefile identifies any file matching AI*.cc as a player.

3. This creates an executable file called Game. This executable allows you to run a match using a command like: `./Game Tixer Demo Demo Demo -s 30 -i default.cnf -o default.out`

* In this case, this runs a match with random seed 30 where three instances of the player "Demo", and one of "Tixer" play with the parameters defined in default.cnf (the default parameters). The output of this match is redirected to the file default.out.

* To watch a match, open the viewer file viewer.html with your browser and load the file default.out. Or alternatively use the script viewer.sh, `viewer.sh default.out`.

## Strategy
Tixer bot uses a capture&defend strategy, following these steps:

1. At the start of the game each Ork searches for the nearest city/path and goes there.

1. Once there, the Ork with more life stays on the city defending the city.
    * Basically, if someone with less life enters on the city, the defender goes and kills him or makes him leave the city.
    * If someone with more life enters, it tries to escape.
    * If there are no orks threatening the city, the defender tries to capture the adjacent paths to earn more points.
    
1. Orks also tries to escape if they are threatened by another Ork.
