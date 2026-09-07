    INCLUDE "constants.bas"

    CONST DIAMONDS = 1
	CONST HEARTS = 2
	CONST CLUBS = 3
	CONST SPADES = 4

    MODE 1
    DEFINE 0,14,cardtop:WAIT
	DEFINE 14,7,cardbot:WAIT

	#pot = 108

    FOR i=0 TO 10:PRINT AT i*20 COLOR BG_DARKGREEN,"                    ":NEXT

    PRINT AT 220 COLOR BG_BLACK + FG_YELLOW,"FOLD CHECK BET   19\276"

    PRINT AT 108 COLOR BG_DARKGREEN + FG_WHITE,"POT"
	PRINT AT 128,<>#pot

    FOR i=0 TO 7
		#addr = players(i)
		p = PEEK(#addr) : REM player position
		op = p
  	    #addr = #addr + 1

        REM plot names
		WHILE (PEEK(#addr)<>255)		
		  #BACKTAB(p) = BG_DARKGREEN + FG_WHITE + PEEK(#addr)*8
		  p = p + 1
		  #addr = #addr + 1
		WEND

		p = op + 20
		#addr = #addr + 1
		WHILE (PEEK(#addr)<>255)
		  card = PEEK(#addr)
		  suit = PEEK(#addr+1)
		  GOSUB print_card
		  p = p + 1
		  #addr = #addr + 2
		WEND

		#col = BG_DARKGREEN
		GOSUB foreground_color	
		#BACKTAB(p) = #col + 275*8
	    #BACKTAB(p+20) = #col + 275*8
	NEXT    

loop:
	GOTO loop

print_card: PROCEDURE
    #col = BG_WHITE
    GOSUB foreground_color
	#BACKTAB(p) = #col + (card + 256) * 8
	#BACKTAB(p + 20) = #col + (suit + 270) * 8
    END

foreground_color: PROCEDURE
    IF card > 0 THEN
	  card = card - 1 : REM compensate for suit runs 23456789TJQKA
	  IF suit = DIAMONDS OR suit = HEARTS THEN
	     #col = #col + FG_RED
	  ELSE
		#col = #col + FG_BLACK
	  END IF
	ELSE
	  #col = #col + FG_BLUE
	END IF

    END

players:
	DATA VARPTR hand_jim(0)
	DATA VARPTR hand_kirk(0)
	DATA VARPTR hand_clyd(0)
	DATA VARPTR hand_meg(0)
	DATA VARPTR hand_fry(0)
	DATA VARPTR hand_thom(0)
	DATA VARPTR hand_player(0)	
	DATA VARPTR hand_hulk(0)

hand_jim:
    DATA 20,"JIM",255 : REM position where handle begins
	DATA 0,0,255 : REM cards on hand

hand_kirk:
    DATA 7,"KIRK",255
	DATA 0,0,255

hand_clyd:
	DATA 34,"CLYD",255
	DATA 11,HEARTS,6,SPADES,3,SPADES,13,DIAMONDS,0,0,255

hand_meg:
	DATA 80,"MEG",255
	DATA 0,0,6,DIAMONDS,11,CLUBS,4,CLUBS,9,CLUBS,255

hand_fry:
	DATA 94,"FRY",255
	DATA 0,0,255

hand_thom:
	DATA 140,"THOM",255
	DATA 0,0,255

hand_player:
    DATA 167,"PLAYER",255
	DATA 3,DIAMONDS,5,DIAMONDS,2,CLUBS,8,SPADES,13,HEARTS,255

hand_hulk:
	DATA 154,"HULK",255
	DATA 0,0,255

cardtop:
	BITMAP "oooooooo"
    BITMAP "oo..o..o"
	BITMAP "o.o..o.."
	BITMAP "o..o..o."
	BITMAP "oo..o..o"
	BITMAP "o.o..o.."
	BITMAP "o..o..o."
	BITMAP "oo..o..o"

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o.oooo.."
	BITMAP "o.....o."
	BITMAP "o..ooo.."
	BITMAP "o.o....."
	BITMAP "o.ooooo."
	BITMAP "o......."
	
	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o.oooo.."
	BITMAP "o.....o."
	BITMAP "o..ooo.."
	BITMAP "o.....o."
	BITMAP "o.oooo.."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o.o...o."
	BITMAP "o.o...o."
	BITMAP "o.ooooo."
	BITMAP "o.....o."
	BITMAP "o.....o."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o.ooooo."
	BITMAP "o.o....."
	BITMAP "o.oooo.."
	BITMAP "o.....o."
	BITMAP "o.oooo.."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o...oo.."
	BITMAP "o..o...."
	BITMAP "o.oooo.."
	BITMAP "o.o...o."
	BITMAP "o..ooo.."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o.ooooo."
	BITMAP "o.....o."
	BITMAP "o....o.."
	BITMAP "o...o..."
	BITMAP "o...o..."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o..ooo.."
	BITMAP "o.o...o."
	BITMAP "o..ooo.."
	BITMAP "o.o...o."
	BITMAP "o..ooo.."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o..ooo.."
	BITMAP "o.o...o."
	BITMAP "o..oooo."
	BITMAP "o.....o."
	BITMAP "o..ooo.."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o.o..o.."
	BITMAP "o.o.o.o."
	BITMAP "o.o.o.o."
	BITMAP "o.o.o.o."
	BITMAP "o.o..o.."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o.....o."
	BITMAP "o.....o."
	BITMAP "o.....o."
	BITMAP "o.o...o."
	BITMAP "o..ooo.."
	BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o..ooo.."
	BITMAP "o.o...o."
	BITMAP "o.o.o.o."
	BITMAP "o.o..o.."
	BITMAP "o..oo.o."
    BITMAP "o......."

	BITMAP "oooooooo"
    BITMAP "o......."
	BITMAP "o.o...o."
	BITMAP "o.o..o.."
	BITMAP "o.ooo..."
	BITMAP "o.o..o.."
	BITMAP "o.o...o."
    BITMAP "o......."	

	BITMAP "oooooooo"
	BITMAP "o......."
	BITMAP "o..ooo.."
	BITMAP "o.o...o."
	BITMAP "o.ooooo."
	BITMAP "o.o...o."
	BITMAP "o.o...o."
	BITMAP "o......."	

cardbot:
	BITMAP "o.o..o.."
    BITMAP "o..o..o."
	BITMAP "oo..o..o"
	BITMAP "o.o..o.."
	BITMAP "o..o..o."
	BITMAP "oo..o..o"
	BITMAP "o.o..o.."
	BITMAP "oooooooo"

	BITMAP "o......."
    BITMAP "o...o..."
	BITMAP "o..ooo.."
	BITMAP "o.ooooo."
	BITMAP "o..ooo.."
	BITMAP "o...o..."
	BITMAP "o......."
	BITMAP "oooooooo"

	BITMAP "o......."
    BITMAP "o..o.o.."
	BITMAP "o.ooooo."
	BITMAP "o.ooooo."
	BITMAP "o..ooo.."
	BITMAP "o...o..."
	BITMAP "o......."
	BITMAP "oooooooo"

	BITMAP "o......."
    BITMAP "o..ooo.."
	BITMAP "o.o.o.o."
	BITMAP "o.ooooo."
	BITMAP "o.o.o.o."
	BITMAP "o...o..."
	BITMAP "o......."
	BITMAP "oooooooo"

	BITMAP "o......."
    BITMAP "o..ooo.."
	BITMAP "o.ooooo."
	BITMAP "o.ooooo."
	BITMAP "o...o..."
	BITMAP "o...o..."
	BITMAP "o......."
	BITMAP "oooooooo"

	BITMAP "o......."
	BITMAP "o......."
	BITMAP "o......."
	BITMAP "o......."
	BITMAP "o......."
	BITMAP "o......."
	BITMAP "o......."
	BITMAP "o......."

	BITMAP ".ooooo.."
	BITMAP "ooo.ooo."
	BITMAP "ooo.ooo."
	BITMAP "ooo...o."
	BITMAP "ooooooo."
	BITMAP "ooooooo."
	BITMAP ".ooooo.."
	BITMAP "........"

