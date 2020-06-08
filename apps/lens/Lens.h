#pragma once

// defines the Lens class for Lens message handling and data reporting.

class Lens
{
private:
	bool m_bQuit;  // used to terminate threads.

public:
	Lens();
	~Lens();
	
	void QuitThreads(){m_bQuit = true;}  // this will cause the threads to terminate.
	bool Quit(){return m_bQuit;}
	
};
