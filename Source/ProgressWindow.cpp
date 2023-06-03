#include "ProgressWindow.h"

#include <stdio.h>
#include <string.h>
#include <StatusBar.h>

ProgressWindow::ProgressWindow(BString* progressTitle)
	:
	BWindow(BRect(200, 100, 500, 100), "ShellIt",
		B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
//		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_QUIT_ON_WINDOW_CLOSE)
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_ASYNCHRONOUS_CONTROLS)
{
	BRect rect = Bounds();

	BView* view = new BView(rect, NULL, B_FOLLOW_ALL, B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	AddChild(view);

	rect = view->Bounds().InsetByCopy(5, 5);
	fStatusBar = new BStatusBar(rect, "status", progressTitle->String(), NULL);
	float width, height;
	fStatusBar->GetPreferredSize(&width, &height);
	fStatusBar->ResizeTo(rect.Width(), height);
	fStatusBar->SetResizingMode(B_FOLLOW_TOP | B_FOLLOW_LEFT_RIGHT);
	view->AddChild(fStatusBar);

	ResizeTo(Bounds().Width(), height + 9);
	CenterOnScreen();

//	Run();
}

ProgressWindow::~ProgressWindow()
{
}

void
ProgressWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgProgressUpdate:
			float percent;
			if (message->FindFloat("percent", &percent) == B_OK)
			{
				fStatusBar->SetTo(percent);
			}
			break;
			
		default:
			BWindow::MessageReceived(message);
	}
}

