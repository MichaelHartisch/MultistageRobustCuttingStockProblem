#include "ScenarioTree.h"

/*Node::Node(int NumSuccessors,int L,bool New){
	this->NumberOfSuccessors=NumSuccessors;
	if(NumberOfSuccessors>0) 
		this->Successors = new Node*[NumberOfSuccessors];
	for (int i=0;i<NumberOfSuccessors;i++)
		this->Successors[i]=0;
	this->Parent=0;
	CurrentSubScenario="";
	this->level=L;
}*/

Node::Node(int NumSuccessors,int L,bool New){
	this->NumberOfSuccessors=NumSuccessors;
	for (int i=0;i<NumberOfSuccessors;i++)
		this->Successors.push_back(-1);
	this->Parent=-1;
	this->Partner=-1;
	this->IsNew=New;
	CurrentSubScenario="";
	this->level=L;
}


Node::~Node(){
	//if(NumberOfSuccessors>0) delete[] Successors;
	//delete Parent;
}
