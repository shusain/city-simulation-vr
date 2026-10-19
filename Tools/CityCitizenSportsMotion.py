"""D88 authored target/body curves for constrained baseball and soccer clips."""
import bpy, math
from mathutils import Vector
from CityCitizenRigControls import seed_hinges, update, move_control

PERIODS={'BaseballPitch':72,'BaseballSwing':72,'SoccerKick':48,'SoccerRun':48,'GoalkeeperSave':48}
PITCH_RELEASE=36;BAT_IMPACT=49;KICK_IMPACT=25;KEEPER_CATCH=25
def smooth(t):
    t=max(0,min(1,t));return t*t*(3-2*t)
def curve(keys,t):
    for (a,va),(b,vb) in zip(keys,keys[1:]):
        if t<=b:return Vector(va).lerp(Vector(vb),smooth((t-a)/(b-a)))
    return Vector(keys[-1][1])
def target(rig,name,point):
    move_control(rig,'CTRL_'+name,Vector(point)-rig.data.bones['CTRL_'+name].head_local)
def body(rig,pelvis,twist=0,lean=0,side=0):
    move_control(rig,'pelvis',pelvis)
    rig.pose.bones['pelvis'].rotation_euler.y=math.radians(twist*.35)
    rig.pose.bones['spine'].rotation_euler.z=math.radians(-lean*.4)
    rig.pose.bones['chest'].rotation_euler.y=math.radians(twist*.65)
    rig.pose.bones['chest'].rotation_euler.z=math.radians(-lean*.6)
    rig.pose.bones['chest'].rotation_euler.x=math.radians(side)
    rig.pose.bones['head'].rotation_euler.y=math.radians(-twist*.75)
    update(rig)
def natural_hands(rig):
    for side in ('L','R'):
        fore=rig.pose.bones['forearm.'+side];control=rig.pose.bones['CTRL_hand.'+side]
        matrix=fore.matrix@fore.bone.matrix_local.inverted()@control.bone.matrix_local
        matrix.translation=control.head;control.matrix=matrix
    update(rig)
def palm(rig,side):
    p=rig.pose.bones['hand.'+side]
    return p.matrix@p.bone.matrix_local.inverted()@Vector(rig['D88_palm_'+side])
def bat_direction(t):
    return curve([(0,(.1,-.22,.97)),(.38,(.1,-.35,.93)),(.52,(.05,-.65,.76)),
                  ((BAT_IMPACT-1)/72,(.30,.95,.10)),(.80,(-.25,.82,.52)),(.92,(.1,-.22,.97)),(1,(.1,-.22,.97))],t).normalized()

def pose(rig,clip,t):
    for p in rig.pose.bones:p.matrix_basis.identity()
    seed_hinges(rig)
    if clip=='BaseballPitch':
        weight=curve([(0,(0,0,-.035)),(.25,(-.035,0,-.035)),(.43,(.02,0,-.04)),
                      (.52,(.07,0,-.05)),(.68,(.10,0,-.07)),(.88,(0,0,-.035)),(1,(0,0,-.035))],t)
        twist=curve([(0,(0,0,0)),(.3,(-28,0,0)),(.43,(-25,0,0)),(.54,(28,0,0)),(.7,(35,0,0)),(.9,(0,0,0)),(1,(0,0,0))],t).x
        lean=curve([(0,(2,0,0)),(.4,(0,0,0)),(.6,(15,0,0)),(.9,(2,0,0)),(1,(2,0,0))],t).x
        body(rig,weight,twist,lean)
        target(rig,'foot.R',(-.11,-.12,.09))
        target(rig,'foot.L',curve([(0,(.12,.12,.09)),(.23,(-.015,.12,.31)),(.35,(.10,.12,.25)),
                                  (.45,(.29,.12,.09)),(.72,(.29,.12,.09)),(.89,(.12,.12,.09)),(1,(.12,.12,.09))],t))
        target(rig,'hand.R',curve([(0,(.10,-.19,1.18)),(.22,(.10,-.14,1.34)),(.40,(-.10,-.24,1.58)),
                                  ((PITCH_RELEASE-1)/72,(.31,-.17,1.56)),(.62,(.35,-.10,1.24)),
                                  (.85,(.10,-.19,1.18)),(1,(.10,-.19,1.18))],t))
        target(rig,'hand.L',curve([(0,(.11,.13,1.18)),(.22,(.22,.12,1.28)),(.40,(.28,.12,1.24)),
                                  (.56,(.13,.13,1.18)),(.85,(.11,.13,1.18)),(1,(.11,.13,1.18))],t))
    elif clip=='BaseballSwing':
        twist=curve([(0,(-12,0,0)),(.42,(-32,0,0)),(.54,(-30,0,0)),
                     ((BAT_IMPACT-1)/72,(26,0,0)),(.8,(35,0,0)),(.94,(-12,0,0)),(1,(-12,0,0))],t).x
        body(rig,(.015,0,-.07),twist,5)
        target(rig,'foot.L',(.12,.14,.09));target(rig,'foot.R',(-.12,-.14,.09))
        target(rig,'hand.R',curve([(0,(.20,-.14,1.20)),(.42,(.10,-.16,1.25)),(.54,(.17,-.15,1.22)),
                                  ((BAT_IMPACT-1)/72,(.27,-.035,1.12)),(.80,(.25,.20,1.23)),
                                  (.94,(.20,-.14,1.20)),(1,(.20,-.14,1.20))],t))
        target(rig,'hand.L',(.20,-.08,1.25))
    elif clip=='SoccerKick':
        body(rig,curve([(0,(0,0,-.04)),(.35,(.02,0,-.045)),(.50,(.055,0,-.045)),
                        (.65,(.07,0,-.07)),(.9,(0,0,-.04)),(1,(0,0,-.04))],t),
             curve([(0,(0,0,0)),(.35,(-12,0,0)),(.6,(15,0,0)),(.9,(0,0,0)),(1,(0,0,0))],t).x,6)
        target(rig,'foot.L',(.10,.12,.09))
        target(rig,'foot.R',curve([(0,(-.075,-.12,.09)),(.30,(-.25,-.12,.19)),(.43,(-.13,-.12,.20)),
                                  (.50,(.25,-.12,.09)),(.65,(.42,-.12,.33)),(.84,(-.075,-.12,.09)),(1,(-.075,-.12,.09))],t))
        target(rig,'hand.L',curve([(0,(.04,.28,1.12)),(.4,(.08,.40,1.20)),(.65,(.02,.38,1.20)),(1,(.04,.28,1.12))],t))
        target(rig,'hand.R',curve([(0,(.06,-.28,1.12)),(.4,(-.06,-.36,1.16)),(.65,(.15,-.34,1.20)),(1,(.06,-.28,1.12))],t))
    elif clip=='SoccerRun':
        phase=t*math.tau*2
        body(rig,(0,0,-.07+.02*math.cos(phase*2)),4*math.sin(phase),9)
        for side,offset,sign in (('L',0,1),('R',.5,-1)):
            u=(t*2+offset)%1
            if u<.6:x=.20-.40*u/.6;z=.09
            else:
                v=(u-.6)/.4;x=-.20+.40*smooth(v);z=.09+.12*math.sin(math.pi*v)
            target(rig,'foot.'+side,(x,sign*.11,z))
            target(rig,'hand.'+side,(.055-.17*math.sin(phase+offset*math.tau),sign*.27,1.11+.025*math.cos(phase)))
    else:
        crouch=curve([(0,(0,0,-.10)),(.28,(0,.025,-.14)),(.50,(.025,.035,-.13)),
                      (.7,(0,0,-.10)),(1,(0,0,-.10))],t)
        body(rig,crouch,0,7,curve([(0,(0,0,0)),(.5,(5,0,0)),(.75,(0,0,0)),(1,(0,0,0))],t).x)
        target(rig,'foot.L',(.05,.17,.09));target(rig,'foot.R',(-.05,-.17,.09))
        for side,sign in (('L',1),('R',-1)):
            target(rig,'hand.'+side,curve([(0,(.18,sign*.27,1.18)),(.40,(.26,sign*.10,1.32)),
                                          (.5,(.26,sign*.07,1.32)),(.7,(.18,sign*.075,1.15)),
                                          (.80,(.18,sign*.075,1.15)),(.95,(.18,sign*.27,1.18)),(1,(.18,sign*.27,1.18))],t))
    update(rig);natural_hands(rig)
    if clip=='BaseballSwing':
        grip=palm(rig,'R')+bat_direction(t)*(.065/rig.scale.x)
        # Solve the second hand's palm point, retaining wrist limits and a real
        # two-bone arm solve. The saved result keys only animator controls.
        for _ in range(40):
            error=grip-palm(rig,'L')
            if error.length<.0002:break
            control=rig.pose.bones['CTRL_hand.L']
            control.location+=control.bone.matrix_local.to_3x3().inverted()@(error*.65)
            update(rig)
    return rig
