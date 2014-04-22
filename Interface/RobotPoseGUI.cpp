#include "RobotPoseGUI.h"
#include <GLdraw/drawMesh.h>
#include <GLdraw/drawgeometry.h>
#include "Modeling/MultiPath.h"
#include <robotics/IKFunctions.h>
#include "Contact/Utils.h"
#include "Planning/RobotCSpace.h"
#include "Planning/RobotConstrainedInterpolator.h"
#include "Planning/RobotTimeScaling.h"
#include "Modeling/MultiPath.h"
#include "Modeling/Interpolate.h"
#include <sstream>

RobotPoseBackend::RobotPoseBackend(RobotWorld* world,ResourceLibrary* library)
: ResourceGUIBackend(world,library){
  settings["cleanContactsNTol"]= 0.01;
  settings["pathOptimize"]["contactTol"] = 0.05;
  settings["pathOptimize"]["outputResolution"] = 0.01; 
  settings["contact"]["forceScale"]= 0.01;
  settings["contact"]["pointSize"]= 5;
  settings["contact"]["normalLength"]= 0.05; 
  settings["linkCOMRadius"] = 0.01;
  settings["flatContactTolerance"] = 0.001;
  settings["selfCollideColor"][0] = 1;
  settings["selfCollideColor"][1] = 0;
  settings["selfCollideColor"][2] = 0;
  settings["selfCollideColor"][3] = 1;
  settings["movieWidth"] = 640;
  settings["hoverColor"] = 1;
  settings["hoverColor"] = 1;
  settings["hoverColor"] = 0;
  settings["hoverColor"] = 1;
  settings["robotColor"][0] = 0.5;
  settings["robotColor"][1] = 0.5;
  settings["robotColor"][2] = 0.5;
  settings["robotColor"][3] = 1;
  settings["movieHeight"] = 480;
  settings["envCollideColor"][0] = 1;
  settings["envCollideColor"][1] = 0.5;
  settings["envCollideColor"][2] = 0;
  settings["envCollideColor"][3] = 1;

  settings["poser"]["color"][0] = 1;
  settings["poser"]["color"][0] = 1;
  settings["poser"]["color"][0] = 0;
  settings["poser"]["color"][0] = 0.5;

  settings["defaultStanceFriction"] = 0.5;
  settings["configResourceColor"][0] = 0.5;
  settings["configResourceColor"][1] = 0.5;
  settings["configResourceColor"][2] = 0.5;
  settings["configResourceColor"][3] = 0.25;
  settings["linkFrameSize"] = 0.2;
  settings["cleanContactsXTol"] = 0.01;
}

void RobotPoseBackend::Start()
{
  if(!settings.read("robotpose.settings")) {
    printf("Didn't read settings from robotpose.settings\n");
    printf("Writing default settings to robotpose_default.settings\n");
    settings.write("robotpose_default.settings");
  }

  WorldGUIBackend::Start();
  robot = world->robots[0].robot;
  cur_link=0;
  cur_driver=0;
  draw_geom = 1;
  draw_bbs = 0;
  draw_com = 0;
  draw_frame = 0;
  pose_ik = 0;
  self_colliding.resize(robot->links.size(),false);   

  for(size_t i=0;i<world->rigidObjects.size();i++)
    objectWidgets[i].Set(world->rigidObjects[i].object,&world->rigidObjects[i].view);
  for(size_t i=0;i<world->robots.size();i++)
    allWidgets.widgets.push_back(&robotWidgets[i]);
  for(size_t i=0;i<world->rigidObjects.size();i++)
    allWidgets.widgets.push_back(&objectWidgets[i]);
  
  poseWidget.Set(world->robots[0].robot,&world->robots[0].view);
  objectWidgets.resize(world->rigidObjects.size());
  

  self_colliding.resize(robot->links.size(),false);
  env_colliding.resize(robot->links.size(),false);

  UpdateConfig();

  MapButtonToggle("pose_ik",&pose_ik);
  MapButtonToggle("draw_geom",&draw_geom);
  MapButtonToggle("draw_bbs",&draw_bbs);
  MapButtonToggle("draw_com",&draw_com);
  MapButtonToggle("draw_frame",&draw_frame);
}

void RobotPoseBackend::UpdateConfig()
{
  for(size_t i=0;i<robotWidgets.size();i++)
    world->robots[i].robot->UpdateConfig(robotWidgets[i].Pose());

  //update collisions
  for(size_t i=0;i<robot->links.size();i++)
    self_colliding[i]=false;
  robot->UpdateGeometry();
  for(size_t i=0;i<robot->links.size();i++) {
    for(size_t j=i+1;j<robot->links.size();j++) {
      if(robot->SelfCollision(i,j)) {
	self_colliding[i]=self_colliding[j]=true;
      }
    }
  }
  for(size_t i=0;i<robot->links.size();i++) {
    for(size_t j=i+1;j<robot->links.size();j++) {
      if(robot->SelfCollision(i,j)) {
	self_colliding[i]=self_colliding[j]=true;
      }
    }
  }
  SendCommand("update_config","");
}


void RobotPoseBackend::RenderWorld()
{
  Robot* robot = world->robots[0].robot;
  ViewRobot& viewRobot = world->robots[0].view;
  //ResourceBrowserProgram::RenderWorld();
  for(size_t i=0;i<world->terrains.size();i++)
    world->terrains[i].view.Draw();
  for(size_t i=0;i<world->rigidObjects.size();i++)
    world->rigidObjects[i].view.Draw();

  if(draw_geom) {
    //set the robot colors
    GLColor robotColor(settings["robotColor"][0],settings["robotColor"][1],settings["robotColor"][2],settings["robotColor"][3]);
    GLColor highlight(settings["hoverColor"][0],settings["hoverColor"][1],settings["hoverColor"][2],settings["hoverColor"][3]);
    GLColor selfcolliding(settings["selfCollideColor"][0],settings["selfCollideColor"][1],settings["selfCollideColor"][2],settings["selfCollideColor"][3]);
    GLColor envcolliding(settings["envCollideColor"][0],settings["envCollideColor"][1],settings["envCollideColor"][2],settings["envCollideColor"][3]);

    viewRobot.SetColors(robotColor);
    for(size_t i=0;i<robot->links.size();i++) {
      if(self_colliding[i]) viewRobot.SetColor(i,selfcolliding);
      if(env_colliding[i]) viewRobot.SetColor(i,envcolliding);
      else if((int)i == cur_link)
	viewRobot.SetColor(i,highlight); 
    }
    allWidgets.DrawGL(viewport);
    viewRobot.Draw();
  }
  else {
    allWidgets.DrawGL(viewport);
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  ResourceGUIBackend::RenderCurResource();
  glDisable(GL_BLEND);
   
  if(draw_com) {
    viewRobot.DrawCenterOfMass();
    Real comSize = settings["linkCOMRadius"];
    for(size_t i=0;i<robot->links.size();i++)
      viewRobot.DrawLinkCenterOfMass(i,comSize);
  }
  if(draw_frame) {
    viewRobot.DrawLinkFrames();
    glDisable(GL_DEPTH_TEST);
    glPushMatrix();
    glMultMatrix((Matrix4)robot->links[cur_link].T_World);
    drawCoords(settings["linkFrameSize"]);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
  }
}


Stance RobotPoseBackend::GetFlatStance()
{
  Robot* robot = world->robots[0].robot;
  Stance s;
  if(poseWidget.ikPoser.poseGoals.empty()) {
    printf("Storing flat ground stance\n");
    ContactFormation cf;
    GetFlatContacts(*robot,settings["flatContactTolerance"],cf);

    Real friction = settings["defaultStanceFriction"];
    for(size_t i=0;i<cf.links.size();i++) {
      //assign default friction
      for(size_t j=0;j<cf.contacts[i].size();j++)
	cf.contacts[i][j].kFriction = friction;
      Hold h;
      LocalContactsToHold(cf.contacts[i],cf.links[i],*robot,h);
      s.insert(h);
    }
  }
  else {
    printf("Storing flat contact stance\n");
    for(size_t i=0;i<poseWidget.ikPoser.poseGoals.size();i++) {
      int link = poseWidget.ikPoser.poseGoals[i].link;
      vector<ContactPoint> cps;
      GetFlatContacts(*robot,link,settings["flatContactTolerance"],cps);
      Real friction = settings["defaultStanceFriction"];
      //assign default friction
      for(size_t j=0;j<cps.size();j++)
	cps[j].kFriction = friction;
      Hold h;
      LocalContactsToHold(cps,link,*robot,h);
      h.ikConstraint = poseWidget.ikPoser.poseGoals[i];
      s.insert(h);
    }
  }
  return s;
}

ResourcePtr RobotPoseBackend::PoserToResource(const string& type)
{
  Robot* robot = world->robots[0].robot;
  if(type == "Config") 
    return MakeResource("",robot->q);
  else if(type == "IKGoal") {
    int ind = poseWidget.ikPoser.ActiveWidget();
    if(ind < 0) {
      printf("Not hovering over any IK widget\n");
      return NULL;
    }
    return MakeResource("",poseWidget.ikPoser.poseGoals[ind]);
  }
  else if(type == "Stance") {
    Stance s = GetFlatStance();
    return MakeResource("",s);
  }
  else if(type == "Grasp") {
    int link = 0;
    cout<<"Which robot link to use? > "; cout.flush();
    cin >> link;
    cin.ignore(256,'\n');
    Grasp g;
    g.constraints.resize(1);
    g.constraints[0].SetFixedTransform(robot->links[link].T_World);
    g.constraints[0].link = link;
    vector<bool> descendant;
    robot->GetDescendants(link,descendant);
    for(size_t i=0;i<descendant.size();i++)
      if(descendant[i]) {
	g.fixedDofs.push_back(i);
	g.fixedValues.push_back(robot->q[i]);
      }
      
    ResourcePtr r=ResourceGUIBackend::CurrentResource();
    const GeometricPrimitive3DResource* gr = dynamic_cast<const GeometricPrimitive3DResource*>((const ResourceBase*)r);
    if(gr) {
      cout<<"Making grasp relative to "<<gr->name<<endl;
      //TODO: detect contacts
	
      RigidTransform T = gr->data.GetFrame();
      RigidTransform Tinv;
      Tinv.setInverse(T);
      g.Transform(Tinv);
    }
    else {
      const RigidObjectResource* obj = dynamic_cast<const RigidObjectResource*>((const ResourceBase*)r);
      if(obj) {
	cout<<"Making grasp relative to "<<obj->name<<endl;
	//TODO: detect contacts
	  
	RigidTransform T = obj->object.T;
	RigidTransform Tinv;
	Tinv.setInverse(T);
	g.Transform(Tinv);
      }
    }
    return MakeResource("",g);
  }
  else {
    fprintf(stderr,"Poser does not contain items of the selected type\n");
    return NULL;
  }
}
void RobotPoseBackend::CleanContacts(Hold& h)
{
  Real ntol = settings["cleanContactsNTol"];
  Real xtol = settings["cleanContactsXTol"];
  CHContacts(h.contacts,ntol,xtol);
}

//BUTTON HANDLING METHODS


bool RobotPoseBackend::OnCommand(const string& cmd,const string& args)
{
  Robot* robot = world->robots[0].robot;
  if(cmd == "poser_to_resource") {
    ResourcePtr r=PoserToResource(args);
    if(r) {
      ResourceGUIBackend::Add(r);
    }
  }
  else if(cmd == "poser_to_resource_overwrite") { 
    ResourcePtr oldr = ResourceGUIBackend::CurrentResource();
    if(!oldr)
      printf("No resource selected\n");
    ResourcePtr r = PoserToResource(r->Type());
    r->name = oldr->name;
    r->fileName = oldr->fileName;
    vector<ResourcePtr >& v=resources->itemsByType[cur_resource_type];
    for(size_t i=0;i<v.size();i++)
      if(v[i]->name == cur_resource_name)
	v[i] = r;
    vector<ResourcePtr>& v2 = resources->itemsByName[cur_resource_name];
    for(size_t i=0;i<v2.size();i++)
      if(v2[i] == oldr) v2[i] = r;
    last_added = r;
    SetLastActive();
  }
  else if(cmd == "resource_to_poser") {
    ResourcePtr r=ResourceGUIBackend::CurrentResource();
    const ConfigResource* rc = dynamic_cast<const ConfigResource*>((const ResourceBase*)r);
    if(rc) {
      poseWidget.SetPose(rc->data);
      robot->NormalizeAngles(poseWidget.linkPoser.poseConfig);
      if(poseWidget.linkPoser.poseConfig != rc->data)
	printf("Warning: config in library is not normalized\n");
      UpdateConfig();
    }
    else {
      const IKGoalResource* rc = dynamic_cast<const IKGoalResource*>((const ResourceBase*)r);
      if(rc) {
	poseWidget.ikPoser.ClearLink(rc->data.link);
	poseWidget.ikPoser.Add(rc->data);
      }
      else {
	const StanceResource* rc = dynamic_cast<const StanceResource*>((const ResourceBase*)r);
	if(rc) {
	  poseWidget.ikPoser.poseGoals.clear();
	  poseWidget.ikPoser.poseWidgets.clear();
	  for(Stance::const_iterator i=rc->stance.begin();i!=rc->stance.end();i++) {
	    Assert(i->first == i->second.ikConstraint.link);
	    poseWidget.ikPoser.Add(i->second.ikConstraint);
	  }
	}
	else {
	  const GraspResource* rc = dynamic_cast<const GraspResource*>((const ResourceBase*)r);
	  if(rc) {
	    poseWidget.ikPoser.poseGoals.clear();
	    poseWidget.ikPoser.poseWidgets.clear();
	    Stance s;
	    rc->grasp.GetStance(s);
	    for(Stance::const_iterator i=s.begin();i!=s.end();i++)
	      poseWidget.ikPoser.Add(i->second.ikConstraint);
	  }
	}
	
      }
    }
  }
  else if(cmd == "create_path") {
    vector<Real> times;
    vector<Config> configs,milestones;
    
    ResourcePtr r=ResourceGUIBackend::CurrentResource();
    const ConfigResource* rc = dynamic_cast<const ConfigResource*>((const ResourceBase*)r);
    if(rc) {
      Config a,b;
      a = rc->data;
      b = robot->q;
      if(a.n != b.n) {
	fprintf(stderr,"Incorrect start and end config size\n");
	return true;
      }
      milestones.resize(2);
      milestones[0] = a;
      milestones[1] = b;
      times.resize(2);
      times[0] = 0;
      times[1] = 1;
    }
    else {
      const ConfigsResource* rc = dynamic_cast<const ConfigsResource*>((const ResourceBase*)r);
      if(rc) {
	milestones = rc->configs;
	times.resize(rc->configs.size());
	for(size_t i=0;i<rc->configs.size();i++)
	  times[i] = Real(i)/(rc->configs.size()-1);
      }
      else {
	return true;
      }
    }
    /*
      if(poseWidget.Constraints().empty()) {
      //straight line interpolator
      configs = milestones;
      }
      else {
      Robot* robot=world->robots[0].robot;
      Timer timer;
      if(!InterpolateConstrainedPath(*robot,milestones,poseWidget.Constraints(),configs,1e-2)) return;
      
      //int numdivs = (configs.size()-1)*10+1;
      int numdivs = (configs.size()-1);
      vector<Real> newtimes;
      vector<Config> newconfigs;
      printf("Discretizing at resolution %g\n",1.0/Real(numdivs));
      SmoothDiscretizePath(*robot,configs,numdivs,newtimes,newconfigs);
      cout<<"Smoothed to "<<newconfigs.size()<<" milestones"<<endl;
      cout<<"Total time "<<timer.ElapsedTime()<<endl;
      swap(times,newtimes);
      swap(configs,newconfigs);
      }
      ResourceGUIBackend::Add("",times,configs);
    */
    MultiPath path;
    path.SetMilestones(milestones);
    path.SetIKProblem(poseWidget.Constraints());
    ResourceGUIBackend::Add("",path);
    ResourceGUIBackend::SetLastActive(); 
    ResourceGUIBackend::viewResource.pathTime = 0;
  }
  else if(cmd == "discretize_path") {
    int num;
    stringstream ss(args);
    ss>>num;
    ResourcePtr r=ResourceGUIBackend::CurrentResource();
    const ConfigsResource* cp = dynamic_cast<const ConfigsResource*>((const ResourceBase*)r);
    if(cp) {
      for(size_t i=0;i<cp->configs.size();i++) {
	stringstream ss;
	ss<<cp->name<<"["<<i+1<<"/"<<cp->configs.size()<<"]";
	Add(ss.str(),cp->configs[i]);
      }
      ResourceGUIBackend::SetLastActive(); 
    }
    const LinearPathResource* lp = dynamic_cast<const LinearPathResource*>((const ResourceBase*)r);
    if(lp) {
      for(int i=0;i<num;i++) {
	Real t = Real(lp->times.size()-1)*Real(i+1)/(num+1);
	int seg = (int)Floor(t);
	Real u = t - Floor(t);
	Config q;
	Assert(seg >= 0 && seg+1 <(int)lp->milestones.size());
	Interpolate(*robot,lp->milestones[seg],lp->milestones[seg+1],u,q);
	stringstream ss;
	ss<<lp->name<<"["<<i+1<<"/"<<num<<"]";
	Add(ss.str(),q);
      }
      ResourceGUIBackend::SetLastActive(); 
    }
    const MultiPathResource* mp = dynamic_cast<const MultiPathResource*>((const ResourceBase*)r);
    if(mp) {
      Real minTime = 0, maxTime = 1;
      if(mp->path.HasTiming()) {
	minTime = mp->path.sections.front().times.front();
	maxTime = mp->path.sections.back().times.back();
      }
      Config q;
      for(int i=0;i<num;i++) {
	Real t = minTime + (maxTime - minTime)*Real(i+1)/(num+1);
	EvaluateMultiPath(*robot,mp->path,t,q);
	stringstream ss;
	ss<<mp->name<<"["<<i+1<<"/"<<num<<"]";
	Add(ss.str(),q);
      }
      ResourceGUIBackend::SetLastActive(); 
    }
  }
  else if(cmd == "optimize_path") {
    ResourcePtr r=ResourceGUIBackend::CurrentResource();
    const LinearPathResource* lp = dynamic_cast<const LinearPathResource*>((const ResourceBase*)r);
    if(lp) {
      vector<double> newtimes;
      vector<Config> newconfigs;
      if(!TimeOptimizePath(*robot,lp->times,lp->milestones,0.01,newtimes,newconfigs)) {
	fprintf(stderr,"Error optimizing path\n");
	return true;
      }
      ResourceGUIBackend::Add("",newtimes,newconfigs);
      ResourceGUIBackend::SetLastActive(); 
      ResourceGUIBackend::viewResource.pathTime = 0;
    }
    const MultiPathResource* mp = dynamic_cast<const MultiPathResource*>((const ResourceBase*)r);
    if(mp) {
      Real xtol = settings["pathOptimize"]["contactTol"];
      Real dt = settings["pathOptimize"]["outputResolution"];
      MultiPath path = mp->path;
      if(!GenerateAndTimeOptimizeMultiPath(*robot,path,xtol,dt)) {
	fprintf(stderr,"Error optimizing path\n");
	return true;
      }
      ResourceGUIBackend::Add("",path);
      ResourceGUIBackend::SetLastActive(); 
      ResourceGUIBackend::viewResource.pathTime = 0;
    }
    const ConfigsResource* rc = dynamic_cast<const ConfigsResource*>((const ResourceBase*)r);
    if(rc) {
      MultiPath path;
      path.sections.resize(1);
      path.sections[0].milestones = rc->configs;
      path.SetIKProblem(poseWidget.Constraints(),0);
      Real xtol = settings["pathOptimize"]["contactTol"];
      Real dt = settings["pathOptimize"]["outputResolution"];
      if(!GenerateAndTimeOptimizeMultiPath(*robot,path,xtol,dt)) {
	fprintf(stderr,"Error optimizing path\n");
	return true;
      }
      ResourceGUIBackend::Add("",path);
      ResourceGUIBackend::SetLastActive(); 
      ResourceGUIBackend::viewResource.pathTime = 0;	  
    }
  }
  else if(cmd == "store_flat_contacts") {
    Stance s = GetFlatStance();
    ResourcePtr r = MakeResource("",s);
    if(r) {
      ResourceGUIBackend::Add(r);
      ResourceGUIBackend::SetLastActive();
    }
  }
  else if(cmd == "clean_contacts") {
    ResourcePtr r=ResourceGUIBackend::CurrentResource();
    const StanceResource* sp = dynamic_cast<const StanceResource*>((const ResourceBase*)r);
    if(sp) {
      Stance s=sp->stance;
      for(Stance::iterator i=s.begin();i!=s.end();i++)
	CleanContacts(i->second);
      ResourcePtr r = MakeResource(sp->name+"_clean",s);
      if(r) {
	ResourceGUIBackend::Add(r);
	ResourceGUIBackend::SetLastActive();
      }
    }
    const HoldResource* hp = dynamic_cast<const HoldResource*>((const ResourceBase*)r);
    if(hp) {
      Hold h = hp->data;
      CleanContacts(h);
      ResourcePtr r = MakeResource(hp->name+"_clean",h);
      if(r) {
	ResourceGUIBackend::Add(r);
	ResourceGUIBackend::SetLastActive();
      }
    }
  }
  else {
    return ResourceGUIBackend::OnCommand(cmd,args);
  }
  SendRefresh();
  return true;
}
