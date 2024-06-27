/*xsd-x selection daemon.
allows to paste clipboard selection even though its owner has died.
build: cc -o xsd xsd.c -lX11 -lXfixes
dep: libxfixes*/
#include<X11/Xlib.h>
#include<X11/extensions/Xfixes.h>
#include<signal.h>
#include<stdlib.h>
#include<stdio.h>
static Atom CLP;//clipboard atom
static Atom UTF;//for utf8 format
static Atom STR;//for string format
static Atom XSTR;//for x string format
static Atom TXT;//for text format
static Atom SEL;//arbitrary atom for our own window to store selection data in
/*targets atom is needed when some lovely client firstly asks us about
targets(formats) we're able to send selection data in*/
static Atom TGT;
static unsigned char* sel=NULL;
static Display* dpl;
static Window w;

void
selntf(XSelectionRequestEvent* srev,Atom prop)//selection notify
{
XSelectionEvent snev;
snev.type=SelectionNotify;
snev.serial=srev->serial;
snev.send_event=srev->send_event;
snev.display=srev->display;
snev.requestor=srev->requestor;
snev.selection=srev->selection;
snev.target=srev->target;
snev.property=prop;
snev.time=srev->time;
XSendEvent(srev->display,srev->requestor,False,NoEventMask,(XEvent*)&snev);
};

int
main()
{
/*dummy values exist only to satisfy XGetWindowProperty function interface.
they do not carry any valuable information*/
int di;Atom tp,da;
unsigned long sz,dul;//selection data size(length)
Atom tgts[]={UTF,XSTR,STR,TXT};//all the selection formats we'll support
XEvent ev;
XFixesSelectionNotifyEvent* fsnev;//xfixes selection notify event
XSelectionRequestEvent* srev;//selection request event
int evbs,erbs;//event base and error base for xfixes.
Window rt;
dpl=XOpenDisplay(NULL);
if(dpl==NULL){dprintf(2,"can't open display.\n");return 1;};
rt=DefaultRootWindow(dpl);
w=XCreateSimpleWindow(dpl,rt,0,0,1,1,0,0,0);
CLP=XInternAtom(dpl,"CLIPBOARD",False);
UTF=XInternAtom(dpl,"UTF8_STRING",False);
STR=XInternAtom(dpl,"STRING",False);
XSTR=XInternAtom(dpl,"XA_STRING",False);
TXT=XInternAtom(dpl,"TEXT",False);
TGT=XInternAtom(dpl,"TARGETS",False);
SEL=XInternAtom(dpl,"SEL",False);
XFixesQueryExtension(dpl,&evbs,&erbs);
XFixesSelectSelectionInput(dpl,rt,CLP,
XFixesSetSelectionOwnerNotifyMask|XFixesSelectionClientCloseNotifyMask
|XFixesSelectionWindowDestroyNotifyMask);
while(1){
XNextEvent(dpl,&ev);
if(ev.type-evbs==XFixesSelectionNotify){
	fsnev=(XFixesSelectionNotifyEvent*)(&ev);
	switch(fsnev->subtype){
	case XFixesSetSelectionOwnerNotify:{
	//nab clipboard selection and save it in case owner died
	//request selection from window owns it right now in UTF format
	XConvertSelection(dpl,CLP,UTF,SEL,w,CurrentTime);
	break;
	}
	case XFixesSelectionWindowDestroyNotify:
	case XFixesSelectionClientCloseNotify:{
	do{XSetSelectionOwner(dpl,CLP,w,CurrentTime);}
	while(XGetSelectionOwner(dpl,CLP)!=w);
	}
	}
}
else if(ev.type==SelectionNotify){//response to our request to read the selection
	XSelectionEvent* snev=&(ev.xselection);
	/*it basically means that selection owner isn't capable of converting
	selection data to utf8 format hence our property we asked him to put the
	resulting data in is empty*/
	if(snev->property==None){
	dprintf(2,"can't convert selection.\n");return 1;
	}
	else//selection ownew has successfully put clipboard selection data into our windows property
	{
	/*make this request in order to obtain data size*/
	XGetWindowProperty(dpl,w,SEL,0,0,False,AnyPropertyType,&tp,&di,&dul,
&sz, &sel);
	/*use data size has just been figured out to finally get selection data into sel variable*/
	XGetWindowProperty(dpl,w,SEL,0,sz,False,AnyPropertyType,&da,
&di,&dul,&dul,&sel);
	}
}
else if(ev.type==SelectionRequest){
	srev=(XSelectionRequestEvent*)(&ev);
	if(srev->property==None)
	{
		selntf(srev,None);//deny selection request
	}
	if(srev->target==TGT)
	{
		/*lovely put targets in requestor's window property for him to chose one
		and perform a next request asking us to convert selection to the format he has chosen*/
		XChangeProperty(srev->display,srev->requestor,srev->property,srev->target,
		32,PropModeReplace,(unsigned char*)tgts,4);
		selntf(srev,srev->property);//notify requestor that we've put targets array in his property
	}
	else if((srev->target==STR)||(srev->target==XSTR)
	||(srev->target==UTF)||(srev->target==TXT))
	{
		XChangeProperty(srev->display,srev->requestor,srev->property,srev->target,8,PropModeReplace,sel,sz);
		selntf(srev,srev->property);
	}
	else
	{
		selntf(srev,None);//deny selection request
	}
}
};
return 0;
};