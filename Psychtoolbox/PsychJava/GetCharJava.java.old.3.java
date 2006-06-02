/*
 * GetCharJava.java is a 1.4 example that requires
 * no other files.
 *
 * Allen.Ingling@nyu.edu
 *
 * Derived from Sun's Java demo KeyEventDemo.java available here:
 * http://java.sun.com/docs/books/tutorial/uiswing/events/example-1dot4/KeyEventDemo.java
 */
 
 
 /* HISTORY
 
	2/2006		awi		Wrote it.
	3/28/2006	awi		Added circular buffer and buffer overflow detection. 
	
 
 /*
 
	 To Do:
 
	1. *COMPLETE* Add a method wich detects focus loss so that we can restore focus within the GetChar.m wrapper when it is lost.
	2. *COMPLETE* the ring buffer implementation; Modify .m wrappers to handle -1 overflow flag returned by numChars, peekChar and getChar methods.
	
	3. Test that it works during WaitSecs.  Add that test to GetCharTest.
	4. Complete the final section of Getchar Test which tests for interaction with mouse clicks on fsw.
	5. Write EventAvail. Check svn logs to see if its gone missing, or never existed in OS X.    
	6. Test it under priority to see if it disturbs blit timing.
	7. Change the window style to get rid of the close button because that kills MATLAB instantly without prompting for file save. 
	8. Try to set Alpha to make the window invisible.  Or to that some other way.
	9. Remove the window's contents, it does not need to display that.
	10. Return a timestamp and reconcile units with GetSecs.
	11. Right an installer to set the Java path instead of issuing instructions.
	12. Try recording which window had focus before setting it to the GetChar window, then restore focus after we get the character. A general-purpose cocoa mex file might
		work for this.
 
 
 */
 
 /*
 
	NOTES
	
	3/9/2006	awi		It looks like we can wire straight into the queue without using a window.  See documentation for KeyEventDispater here:
						http://java.sun.com/j2se/1.5.0/docs/api/java/awt/KeyEventDispatcher.html
						
	3/9/2006	awi		Useful documentation:
	
						JFrame:
						http://java.sun.com/j2se/1.5.0/docs/api/java/awt/Frame.html
						
						
						
*/ 

import javax.swing.*;
import java.awt.event.*;
import java.awt.BorderLayout;
import java.awt.Dimension;

public class GetCharJava extends JPanel 
                          implements KeyListener,
                                     ActionListener {
    JTextArea displayArea;
    JTextField typingArea;
    static final String newline = "\n";
	static JFrame frame;
	static char[] characterBuffer;
	int characterBufferTail=0;
	int characterBufferHead=0;
	int characterBufferHeadStepped=0;
	boolean bufferOverflowFlag=false;	
	char emptyChar='\0';
	int	queueSizeChars=100; //make this small to test the ring buffer and overflow warnings

    public GetCharJava() {
        super(new BorderLayout());

        JButton button = new JButton("Clear");
        button.addActionListener(this);

        typingArea = new JTextField(20);
        typingArea.addKeyListener(this);

        //Uncomment this if you wish to turn off focus
        //traversal.  The focus subsystem consumes
        //focus traversal keys, such as Tab and Shift Tab.
        //If you uncomment the following line of code, this
        //disables focus traversal and the Tab events will
        //become available to the key event listener.
        //typingArea.setFocusTraversalKeysEnabled(false);

        displayArea = new JTextArea();
        displayArea.setEditable(false);
        JScrollPane scrollPane = new JScrollPane(displayArea);
        scrollPane.setPreferredSize(new Dimension(375, 125));

        add(typingArea, BorderLayout.PAGE_START);
        add(scrollPane, BorderLayout.CENTER);
        add(button, BorderLayout.PAGE_END);
		
		//setup the ring buffer by allocating memory
		characterBuffer= new char[queueSizeChars];   
    }

    /** Handle the key typed event from the text field. */
    public void keyTyped(KeyEvent e) {
        displayInfo(e, "KEY TYPED: ");
		recordKey(e);	
    }

    /** Handle the key pressed event from the text field. */
    public void keyPressed(KeyEvent e) {
        displayInfo(e, "KEY PRESSED: ");
    }

    /** Handle the key released event from the text field. */
    public void keyReleased(KeyEvent e) {
        displayInfo(e, "KEY RELEASED: ");
    }

    /** Handle the button click. */
    public void actionPerformed(ActionEvent e) {
        //Clear the text components.
        displayArea.setText("");
        typingArea.setText("");

        //Return the focus to the typing area.
        typingArea.requestFocusInWindow();
    }
	

	public void windowFocusOn(){
		frame.toFront();
	}
	
	public void typingAreaFocusOn(){
		typingArea.requestFocusInWindow();
	}
	
	public void windowFocusOff(){
		frame.toBack();
	}
	

	public String getTitle(){
		return(frame.getTitle());
	}
	
	public void sayHello(){
		System.out.println("Hello World!");
	}
	

    /*
     * We have to jump through some hoops to avoid
     * trying to print non-printing characters 
     * such as Shift.  (Not only do they not print, 
     * but if you put them in a String, the characters
     * afterward won't show up in the text area.)
     */
    protected void displayInfo(KeyEvent e, String s){
        String keyString, modString, tmpString,
               actionString, locationString;

        //You should only rely on the key char if the event
        //is a key typed event.
        int id = e.getID();
        if (id == KeyEvent.KEY_TYPED) {
            char c = e.getKeyChar();
            keyString = "key character = '" + c + "'";
        } else {
            int keyCode = e.getKeyCode();
            keyString = "key code = " + keyCode
                        + " ("
                        + KeyEvent.getKeyText(keyCode)
                        + ")";
        }

        int modifiers = e.getModifiersEx();
        modString = "modifiers = " + modifiers;
        tmpString = KeyEvent.getModifiersExText(modifiers);
        if (tmpString.length() > 0) {
            modString += " (" + tmpString + ")";
        } else {
            modString += " (no modifiers)";
        }

        actionString = "action key? ";
        if (e.isActionKey()) {
            actionString += "YES";
        } else {
            actionString += "NO";
        }

        locationString = "key location: ";
        int location = e.getKeyLocation();
        if (location == KeyEvent.KEY_LOCATION_STANDARD) {
            locationString += "standard";
        } else if (location == KeyEvent.KEY_LOCATION_LEFT) {
            locationString += "left";
        } else if (location == KeyEvent.KEY_LOCATION_RIGHT) {
            locationString += "right";
        } else if (location == KeyEvent.KEY_LOCATION_NUMPAD) {
            locationString += "numpad";
        } else { // (location == KeyEvent.KEY_LOCATION_UNKNOWN)
            locationString += "unknown";
        }

        displayArea.append(s + newline
                           + "    " + keyString + newline
                           + "    " + modString + newline
                           + "    " + actionString + newline
                           + "    " + locationString + newline);
        displayArea.setCaretPosition(displayArea.getDocument().getLength());
    }
	

	//we add new characters to the head and remove them from the tail
	protected void recordKey(KeyEvent e){
        int id = e.getID();
        if (id == KeyEvent.KEY_TYPED) {
			characterBuffer[characterBufferHead]= e.getKeyChar();
			characterBufferHeadStepped= (characterBufferHead+1) % queueSizeChars; 
			if(characterBufferHeadStepped == characterBufferTail)
				bufferOverflowFlag=true;
			else
				characterBufferHead= characterBufferHeadStepped;																		
		}
	}
									 


	public double numChars(){
		if(bufferOverflowFlag)
			return(-1.0);
		else
			return((double)numCharsLocal());
	}
 


	public int numCharsLocal(){
		if(characterBufferTail > characterBufferHead)
			return(queueSizeChars-characterBufferTail);
		else
			return(characterBufferHead-characterBufferTail);
	}



	public double peekChar(){
		if(bufferOverflowFlag)
			return(-1.0);
		else if(numCharsLocal() > 0){
			return((double)characterBuffer[characterBufferTail]);
		}else{
			return((double)emptyChar);		
		}	
    }
	
	
/*
	public char getChar(){
		if(numChars() > 0){
			char tempChar= characterBuffer[characterBufferTail];
			characterBufferTail= characterBufferTail+1;
			return(tempChar);
		}else{
			return(emptyChar);		
		}	
    }
*/
	public double getChar(){
		if(bufferOverflowFlag)
			return(-1.0);
		else if(numCharsLocal() > 0){
			char tempChar= characterBuffer[characterBufferTail];
			characterBufferTail= (characterBufferTail+1) % queueSizeChars;
			return((double)tempChar);
		}else{
			return((double)emptyChar);		
		}	
    }

	
	public void flushChars(){
		bufferOverflowFlag=false;
		characterBufferHead=characterBufferTail;
	}
	
	public boolean isWindowFocused(){
		return(frame.isFocused());
	}
	
	public double getQueueLength(){
		return((double)(queueSizeChars-1));
	}
	
	
    /**
     * Create the GUI and show it.  For thread safety,
     * this method should be invoked from the
     * event-dispatching thread.
     */
    //private static void createAndShowGUI() {
	/*
     public static void createAndShowGUI() {
        //Make sure we have nice window decorations.
        JFrame.setDefaultLookAndFeelDecorated(true);

        //Create and set up the window.
        JFrame frame = new JFrame("GetCharJava");
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        //Create and set up the content pane.
        JComponent newContentPane = new GetCharJava();
        newContentPane.setOpaque(true); //content panes must be opaque
        frame.setContentPane(newContentPane);

        //Display the window.
        frame.pack();
        frame.setVisible(true);
    }
	*/
	
	
	public static double getVersion(){
		return(0.1);
	}
	
	 public static JComponent createAndShowGUI() {
	 		
		//Make sure we have nice window decorations.
		JFrame.setDefaultLookAndFeelDecorated(true);

		//Create and set up the window.
//		JFrame frame = new JFrame("GetCharJava");
		frame = new JFrame("GetCharJava");
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

		//Create and set up the content pane.
		JComponent newContentPane = new GetCharJava();
		newContentPane.setOpaque(true); //content panes must be opaque
		frame.setContentPane(newContentPane);

		//Display the window.
		frame.pack();
		frame.setVisible(true);
		
		//return this object
		return(newContentPane);
	}

/*
    public static void main(String[] args) {
        //Schedule a job for the event-dispatching thread:
        //creating and showing this application's GUI.
        javax.swing.SwingUtilities.invokeLater(new Runnable() {
            public void run() {
                createAndShowGUI();
            }
        });
    }
*/


    public static void main() {
        //Schedule a job for the event-dispatching thread:
        //creating and showing this application's GUI.
		
        javax.swing.SwingUtilities.invokeLater(new Runnable() {
            public void run() {
                createAndShowGUI();
            }
        });
    }
	


}
