#ifndef PROGRESS_WINDOW_H
#define PROGRESS_WINDOW_H

#include <Window.h>

class BStatusBar;

enum {
	kMsgProgressUpdate = 'pwPU'
};

class ProgressWindow : public BWindow {
public:
						ProgressWindow(BString *progressTitle);
	virtual				~ProgressWindow();
	virtual void 		MessageReceived(BMessage* message);

private:
	BStatusBar*			fStatusBar;
};


#endif	// PROGRESS_WINDOW_H
