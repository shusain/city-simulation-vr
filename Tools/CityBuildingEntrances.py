"""D103 study entrance recipes; use existing slots and opaque glass artwork.

No scene/file work on import. The caller supplies its mesh builder and owns
review/export. Face ranges describe the actual generated parts for inspection.
"""


def glass(b,x0,x1,y,z0,z1):
    b.face([(x0,y,z0),(x1,y,z0),(x1,y,z1),(x0,y,z1)],4,(1,1,1,0),[(0,0),(1,0),(1,1),(0,1)])


def framed_glass(b,x0,x1,y,z0,z1,frame=.075):
    glass(b,x0+frame,x1-frame,y+.035,z0+frame,z1-frame)
    for x in (x0+frame/2,x1-frame/2):
        b.box((x,y,(z0+z1)/2),(frame,.10,z1-z0),1)
    for z in (z0+frame/2,z1-frame/2):
        b.box(((x0+x1)/2,y,z),(x1-x0,.10,frame),1)


def store_entrance(b,variant):
    start=len(b.faces)
    floor=.16
    top=floor+2.35
    y=-7.16 if variant!=3 else -7.84
    pair=2.50 if variant!=2 else 2.60
    # Door leaves, center seam and obvious pull bars on swing-door variants.
    for x0,x1 in ((-pair/2,-.025),(.025,pair/2)):
        framed_glass(b,x0,x1,y,floor,top)
        if variant!=2:
            x=-.15 if x1<0 else .15
            b.box((x,y-.10,1.35),(.045,.075,.56),1)
            for z in (1.12,1.58):b.box((x,y-.065,z),(.08,.12,.04),1)
        if variant==1:
            b.box(((x0+x1)/2,y-.012,floor+.17),(x1-x0-.12,.08,.28),1)
    style=('double glass with transom','double glass with sidelights/kickplates',
           'automatic sliding pair','glazed double-door vestibule')[variant]
    width=pair
    if variant in (1,2):
        side=.60 if variant==1 else .95
        for x0,x1 in ((-pair/2-side,-pair/2),(pair/2,pair/2+side)):
            framed_glass(b,x0,x1,y,floor,top)
        width=pair+side*2
    if variant==0:
        framed_glass(b,-1.38,1.38,y,top+.06,top+.72)
        width=2.76
    if variant==2:
        b.box((0,y-.035,top+.16),(width+.18,.24,.26),1)
        # Door-guide rail and a compact sensor over the central overlap.
        b.box((0,y-.18,top+.20),(.28,.12,.10),1)
        b.box((0,y-.065,floor+.015),(pair+.08,.18,.035),1)
    else:
        b.box((0,y,top+.03),(width+.14,.18,.13),1)
    if variant==3:
        # Side glazing joins the forward door plane back to the actual facade.
        for x in (-1.40,1.40):
            points=[(x,y,floor+.08),(x,-7.02,floor+.08),(x,-7.02,top-.05),(x,y,top-.05)]
            if x<0:points.reverse()
            b.face(points,4,(1,1,1,0),[(0,0),(1,0),(1,1),(0,1)])
            for py in (y,-7.02):b.box((x,py,(floor+top)/2),(.10,.10,top-floor),1)
            for z in (floor+.05,top-.03):b.box((x,(y-7.02)/2,z),(.10,-7.02-y,.10),1)
        b.box((0,(y-7.02)/2,top+.05),(2.95,-7.02-y+.12,.16),1)
        width=2.95
    b.box((0,y-.10,floor+.015),(width+.08,.32,.03),1)
    return dict(style=style,door_leaf_count=2,personnel_clear_width_m=pair-.20,
                door_height_m=2.35,floor_m=floor,door_front_y_m=y,
                faces=list(range(start,len(b.faces))),
                detail_bounds_m=[[-width/2-.15,y-.26,0],[width/2+.15,-7.0,3.40]])


def personnel_door(b,variant):
    start=len(b.faces)
    x,y,floor=5.20,-7.15,.16
    width,height=1.10,2.05
    b.box((x,y+.025,floor+height/2),(width,.13,height),1)
    for px in (x-width/2-.055,x+width/2+.055):
        b.box((px,y-.03,floor+height/2),(.11,.18,height+.08),4)
    b.box((x,y-.03,floor+height+.055),(width+.22,.18,.11),4)
    b.box((x,y-.11,floor+.02),(width+.22,.36,.04),1)
    # Steel lever, latch plate and visible hinge barrels.
    b.box((x+.37,y-.09,1.12),(.06,.055,.20),4)
    b.box((x+.30,y-.13,1.16),(.23,.075,.045),1)
    for z in (.55,1.65):b.box((x-.56,y-.05,z),(.035,.07,.13),1)
    if variant in (0,2,3):
        framed_glass(b,x-.24,x+.24,y-.09,1.36,1.95,.045)
    # The personnel path reaches the same front road edge beside freight access.
    b.box((x,-8.73,.08),(1.45,3.34,.16),1)
    return dict(center_x_m=x,front_y_m=y,width_m=width,height_m=height,floor_m=floor,
                faces=list(range(start,len(b.faces))),path_bounds_m=[[x-.725,-10.40,0],[x+.725,-7.06,.16]])


def freight_door(b,x,width,floor,height,roller=False,glazed=False):
    start=len(b.faces)
    y=-7.17
    b.box((x,y+.055,floor+height/2),(width,.16,height),1)
    for px in (x-width/2-.09,x+width/2+.09):
        b.box((px,y-.04,floor+height/2),(.18,.23,height+.05),1)
    b.box((x,y-.04,floor+height+.09),(width+.36,.24,.18),1)
    spacing=.25 if roller else .78
    z=floor+spacing
    while z<floor+height-.10:
        if not (glazed and floor+height-.96<z<floor+height-.38):
            b.box((x,y-.035,z),(width-.10,.055,.035 if roller else .055),1)
        z+=spacing
    if glazed:
        count=4 if width>4 else 2
        for i in range(count):
            center=x+(i-(count-1)/2)*width/(count+.4)
            framed_glass(b,center-.39,center+.39,y-.09,floor+height-.90,floor+height-.45,.04)
    b.box((x,y-.07,floor+.025),(width+.18,.26,.05),1)
    return dict(center_x_m=x,width_m=width,height_m=height,floor_m=floor,front_y_m=y,
                style='roller shutter' if roller else 'glazed sectional' if glazed else 'sectional',
                faces=list(range(start,len(b.faces))))


def industrial_entrances(b,variant):
    person=personnel_door(b,variant)
    start=len(b.faces)
    dock=None
    if variant==3:
        delivery=[freight_door(b,x,2.60,.08,4.45,glazed=True) for x in (-1.47,1.47)]
        b.box((0,-7.22,2.33),(.20,.22,4.50),1)
        style='two workshop sectional bays'
    else:
        floor=1.05 if variant==2 else .08
        delivery=[freight_door(b,0,5.65,floor,3.65 if variant==2 else 4.55,
                               roller=variant in (1,2),glazed=variant==0)]
        style=('glazed sectional garage','warehouse roller shutter','raised loading dock')[variant]
    if variant==2:
        b.box((0,-8.26,.525),(6.40,2.42,1.05),1)
        b.box((0,-9.49,1.06),(5.60,.16,.04),1)
        # Dock bumpers are attached to the vertical front apron, outside steps.
        for x in (-2.40,2.40):b.box((x,-9.56,.72),(.34,.20,.58),1)
        for i in range(5):
            step=.21*(i+1)
            b.box((-4.8425+i*.365,-8.30,step/2),(.365,1.10,step),1)
        dock=dict(platform_top_m=1.05,platform_bounds_m=[[-3.20,-9.47,0],[3.20,-7.05,1.05]],
                  step_count=5,step_rise_m=.21)
    else:
        # Low protective bollards flank freight access, clear of the person door.
        for x in (-3.38,3.38):b.box((x,-8.02,.46),(.24,.24,.92),2)
    return dict(style=style,personnel=person,freight=delivery,dock=dock,
                freight_faces=list(range(start,len(b.faces))),
                detail_bounds_m=[[-5.35,-10.55,0],[6.0,-6.9,5.30]])
