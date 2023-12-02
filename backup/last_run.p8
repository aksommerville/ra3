pico-8 cartridge // http://www.pico-8.com
version 38
__lua__
-- sitter
-- aksommerville
hwvv={
  0,
  1,1,1,
  2
}
hwvp=1
hjvv={
  5,4,3,3,2,2,2,
  1,1,1,1,1,0,0,0
}
hjvp=#hjvv
btn4pv=false
btn5pv=false
sprites={}
victorytime=0
deadtime=0
mapid=0
gameover=false
levelwrap=false
losscount=0
deadbaby=false

function _init()
  cartdata("sitter_hi_scores")
  reinit()
end

function _update()
  if (gameover) then
    if (btnp(4)) then
      reinit()
    end
  elseif (levelwrap) then
    if (btnp(4)) then
      levelwrap=false
      if (deadbaby) mapid-=1
      next_map()
    end
  else
    world_update()
  end
end

function _draw()
  if (gameover) then
    draw_gameover()
  elseif (levelwrap) then
    draw_levelwrap()
  else
    draw_world()
  end
  draw_chrome()
end





-- clocks
-- tt=total time
-- lr=level record
-- (f)rame, (s)econd, (m)inute
-- splitting out like this bc
-- numbers are 15-bit, we'd run out
tt_f=0
tt_s=0
tt_m=0
lt_f=0
lt_s=0
lt_m=0
tr_f=0
tr_s=0
tr_m=0
lr_f=0
lr_s=0
lr_m=0

function clocks_clear()
  tt_f=0
  tt_s=0
  tt_m=0
  lt_f=0
  lt_s=0
  lt_m=0
  tr_f=peek(0x5e00)
  tr_s=peek(0x5e01)
  tr_m=peek(0x5e02)
  if (tr_f+tr_s+tr_m<1) then
    tr_s=59
    tr_m=59
  end
  lr_f=0
  lr_s=59
  lr_m=59
end

function clocks_clear_level()
  lt_f=0
  lt_s=0
  lt_m=0
  lr_f=peek(0x5e00+mapid*3)
  lr_s=peek(0x5e01+mapid*3)
  lr_m=peek(0x5e02+mapid*3)
  if (lr_f+lr_s+lr_m<1) then
    lr_s=59
    lr_m=59
  end
end

function clocks_write_level_if()
  if (mapid<1) return
  if (lt_m>lr_m) return
  if (lt_m==lr_m) then
    if (lt_s>lr_s) return
    if (lt_s==lr_s) then
      if (lt_f>lr_f) return
    end
  end
  poke(0x5e00+mapid*3,lt_f)
  poke(0x5e01+mapid*3,lt_s)
  poke(0x5e02+mapid*3,lt_m)
end

function clocks_write_total_if()
  if (tt_m>tr_m) return
  if (tt_m==tr_m) then
    if (tt_s>tr_s) return
    if (tt_s==tr_s) then
      if (tt_f>tr_f) return
    end
  end
  poke(0x5e00,tt_f)
  poke(0x5e01,tt_s)
  poke(0x5e02,tt_m)
end

function clocks_tick()
  lt_f+=1
  if (lt_f>=30) then
    lt_f=0
    lt_s+=1
    if (lt_s>=60) then
      lt_s=0
      lt_m+=1
    end
  end
  tt_f+=1
  if (tt_f>=30) then
    tt_f=0
    tt_s+=1
    if (tt_s>=60) then
      tt_s=0
      tt_m+=1
    end
  end
end





function mksprite(col,row)
  local sprite={}
  sprite.x=col*8
  sprite.y=row*8
  sprite.update=sprite_update
  sprite.w=8
  sprite.h=8
  sprite.suspend_collision=false
  sprite.carried=false
  sprite.xiner=0
  sprite.human=false
  return sprite
end

function mkhero(col,row)
  local sprite=mksprite(col,row)
  sprite.tileid=0x10
  sprite.draw=hero_draw
  sprite.update=hero_update
  sprite.onkill=hero_kill
  sprite.y-=6
  sprite.h+=6
  sprite.faceright=false
  sprite.facey=0
  sprite.armsup=false
  sprite.solid=true
  sprite.walkframe=0
  sprite.walkclock=0
  sprite.human=true
  sprite.fragile=true
  return sprite
end

function mkdesmond(col,row)
  local sprite=mksprite(col,row)
  sprite.tileid=0x21
  sprite.pickup=true
  sprite.solid=true
  sprite.human=true
  sprite.fragile=true
  return sprite
end

function mksusie(col,row)
  local sprite=mksprite(col,row)
  sprite.tileid=0x24
  sprite.pickup=true
  sprite.solid=true
  sprite.human=true
  sprite.fragile=true
  sprite.update=susie_update
  sprite.faceright=false
  sprite.animclock=0
  sprite.animframe=0
  return sprite
end

function mktomato(col,row)
  local sprite=mksprite(col,row)
  sprite.tileid=0x20
  sprite.pickup=true
  sprite.solid=true
  sprite.fragile=true
  return sprite
end

function mkorange(col,row)
  local sprite=mksprite(col,row)
  sprite.tileid=0x27
  sprite.pickup=true
  sprite.solid=true
  sprite.fragile=true
  return sprite
end

function mkturnip(col,row)
  local sprite=mksprite(col,row)
  sprite.tileid=0x28
  sprite.pickup=true
  sprite.solid=true
  sprite.fragile=true
  return sprite
end

function mkfire(col,row)
  local sprite=mksprite(col,row)
  sprite.tileid=0x22
  sprite.update=fire_update
  sprite.animclock=0
  return sprite
end

function clear_map()
  local dstp=0x2000
  for i=1,12 do
    memset(dstp,0,16)
    dstp+=128
  end
end

function copy_map_to_scratch(srcx,srcy)
  local dstp=0x2000
  local srcp=0x2000+srcy*128+srcx
  for i=1,12 do
    memcpy(dstp,srcp,16)
    dstp+=128
    srcp+=128
  end
end

function load_sprites()
  for y=0,11 do
    for x=0,15 do
      local v=mget(x,y)
      if (v==0x10) then
        add(sprites,mkhero(x,y))
        mset(x,y,3)
      elseif (v==0x20) then
        add(sprites,mktomato(x,y))
        mset(x,y,3)
      elseif (v==0x21) then
        add(sprites,mkdesmond(x,y))
        mset(x,y,3)
      elseif (v==0x22) then
        add(sprites,mkfire(x,y))
        mset(x,y,3)
      elseif (v==0x24) then
        add(sprites,mksusie(x,y))
        mset(x,y,3)
      elseif (v==0x27) then
        add(sprites,mkorange(x,y))
        mset(x,y,3)
      elseif (v==0x28) then
        add(sprites,mkturnip(x,y))
        mset(x,y,3)
      end
    end
  end
end

function next_map()
  clocks_write_level_if()
  mapid+=1
  clocks_clear_level()
  if ((mapid<1) or (mapid>=40)) then
    -- 0 is the scratch space
    -- 40 is the absolute limit
    mapid=0
    clear_map()
  else
    local mapsrcx=band(mapid,7)*16
    local mapsrcy=band(shr(mapid,3),0xff)*12
    copy_map_to_scratch(mapsrcx,mapsrcy)
  end
  deadbaby=false
  sprites={}
  victorytime=0
  hwvp=1
  hjvp=#hjvv
  load_sprites()
  if (#sprites==0) then
    clocks_write_total_if()
    gameover=true
    mapid=0
  else
    music(0)
  end
end

function reinit()
  mapid=0
  gameover=false
  losscount=0
  clocks_clear()
  next_map()
end





function map_flag_in_rect(x,y,w,h,f)
  local cola=flr(x/8)
  local rowa=flr(y/8)
  local colz=flr((x+w-1)/8)
  local rowz=flr((y+h-1)/8)
  for row=rowa,rowz do
    for col=cola,colz do
      local v=mget(col,row)
      if (fget(v,f)) then
        return true
      end
    end
  end
  return false
end

-- map_flag_in_rect, but only the top edge
function oneway_in_rect(x,y,w,h,f)
  local cola=flr(x/8)
  local rowa=flr(y/8)
  local colz=flr((x+w-1)/8)
  local rowz=flr((y+h-1)/8)
  if (rowa>=rowz) then
    return false
  end
  rowa+=1
  for row=rowa,rowz do
    for col=cola,colz do
      local v=mget(col,row)
      if (fget(v,f)) then
        return true
      end
    end
  end
  return false
end

-- one of (dx,dy) must be zero.
-- returns true if moved fully.
function sprite_move(sprite,dx,dy)
  sprite.x+=dx
  sprite.y+=dy
  local result=true
  
  -- clamp hard to edges
  if (sprite.x<0) then
    sprite.x=0
    result=false
  elseif (sprite.y<0) then
    sprite.y=0
    result=false
  elseif (sprite.x+sprite.w>128) then
    sprite.x=128-sprite.w
    result=false
  elseif (sprite.y+sprite.h>96) then
    sprite.y=96-sprite.h
    result=false
  end
  
  -- non-solid sprites, that's all
  if (not sprite.solid) then
    return result
  end
  
  -- check leading edge against the map
  local tx=sprite.x
  local ty=sprite.y
  local tw=1
  local th=1
  local fixx=sprite.x
  local fixy=sprite.y
  if (dx<0) then
    th=sprite.h
    fixx=band(sprite.x+7,bnot(7))
  elseif (dx>0) then
    th=sprite.h
    tx+=sprite.w-1
    fixx=flr(band(sprite.x+sprite.w-1,bnot(7))-sprite.w)
  elseif (dy<0) then
    tw=sprite.w
    fixy=band(sprite.y+7,bnot(7))
  elseif (dy>0) then
    tw=sprite.w
    ty+=sprite.h-1
    fixy=flr(band(sprite.y+sprite.h-1,bnot(7))-sprite.h)
  else
    result=false
  end
  if (map_flag_in_rect(tx,ty,tw,th,0)) then
    sprite.x=fixx
    sprite.y=fixy
    result=false
  end
  if ((dy>0) and oneway_in_rect(tx,ty-dy,tw,dy+1,1)) then
    sprite.y=fixy
    result=false
  end
  
  -- check solid sprites
  sprite.suspend_collision=true
  for other in all(sprites) do
    if (
      (other!=sprite) and
      other.solid and
      not other.suspend_collision and
      (other.x<sprite.x+sprite.w) and
      (other.y<sprite.y+sprite.h) and
      (other.x+other.w>sprite.x) and
      (other.y+other.h>sprite.y)
    ) then
      if (dx<0) then
        sprite_move(other,-1,0)
        sprite.x=other.x+other.w
        result=false
      elseif (dx>0) then
        sprite_move(other,1,0)
        sprite.x=other.x-sprite.w
        result=false
      elseif (dy<0) then
        sprite_move(other,0,-1)
        sprite.y=other.y+other.h
        result=false
      elseif (dy>0) then
        sprite_move(other,0,1)
        sprite.y=other.y-sprite.h
        result=false
      end
    end
  end
  sprite.suspend_collision=false
  
  return result
end



function hero_jump_down(sprite)
  if (
    oneway_in_rect(
      sprite.x,sprite.y+sprite.h-1,
      sprite.w,2,1
    ) and not map_flag_in_rect(
      sprite.x,sprite.y,
      sprite.w,sprite.h,0
    )
  ) then
    sprite.y+=1
    hjvp=#hjvv
    sfx(1)
  end
end

function find_pumpkin(x,y,w,h)
  for sprite in all(sprites) do
    if (
      sprite.pickup and
      sprite.solid and
      (sprite.x<x+w) and
      (sprite.y<y+h) and
      (sprite.x+sprite.w>x) and
      (sprite.y+sprite.h>y)
    ) then
      return sprite
    end
  end
  return nil
end

function toss_all(hero,sprite,dx)
  sprite.carried=false
  sprite.solid=true
  hero.y+=sprite.h
  hero.h-=sprite.h
  if (dx<0) then
    sprite.x=hero.x
    sprite_move(sprite,-8,0)
  elseif (dx>0) then
    sprite.x=hero.x+hero.w-sprite.w
    sprite_move(sprite,8,0)
  end
end

function toss(hero,sprite,dx)
  toss_all(hero,sprite,dx)
  sprite.xiner=dx*24
end

function drop(hero,sprite,dx)
  toss_all(hero,sprite,dx)
end

function toss_up(hero,sprite)
  toss_all(hero,sprite,0)
  sprite_move(sprite,0,-4)
end

function toss_down(hero,sprite)
  toss_all(hero,sprite,0)
  hero.y=sprite.y
  sprite.y=hero.y+hero.h
end

function hero_pickup_or_toss(sprite)
  if (sprite.armsup) then
    sprite.armsup=false
    sfx(3)
    if (sprite.facey<0) then
      toss_up(sprite,sprite.pumpkin)
    elseif (sprite.facey>0) then
      toss_down(sprite,sprite.pumpkin)
    elseif (sprite.faceright) then
      if (btn(1)) then
        toss(sprite,sprite.pumpkin,1)
      else
        drop(sprite,sprite.pumpkin,1)
      end
    else
      if (btn(0)) then
        toss(sprite,sprite.pumpkin,-1)
      else
        drop(sprite,sprite.pumpkin,-1)
      end
    end
    sprite.pumpkin=nil
  else
    local pumpkin=nil
    if (sprite.facey<0) then
      pumpkin=find_pumpkin(
        sprite.x,
        sprite.y-4,
        sprite.w,
        4
      )
    elseif (sprite.facey>0) then
      pumpkin=find_pumpkin(
        sprite.x,
        sprite.y+sprite.h,
        sprite.w,4,
        4
      )
    elseif (sprite.faceright) then
      pumpkin=find_pumpkin(
        sprite.x+sprite.w,
        sprite.y,
        4,sprite.h
      )
    else
      pumpkin=find_pumpkin(
        sprite.x-4,
        sprite.y,
        4,sprite.h
      )
    end
    if (pumpkin) then
      if (map_flag_in_rect(
        sprite.x,sprite.y-pumpkin.h,sprite.w,pumpkin.h,0
      )) then
        return
      end
      sfx(2)
      sprite.armsup=true
      sprite.pumpkin=pumpkin
      pumpkin.carried=true
      pumpkin.solid=false
      if (sprite.facey>0) then
        sprite.x=pumpkin.x
      else
        pumpkin.x=sprite.x+sprite.w/2-pumpkin.w/2
      end
      pumpkin.y=sprite.y-pumpkin.h
      sprite.y-=pumpkin.h
      sprite.h+=pumpkin.h
      if (sprite.facey>0) then
        sprite.y+=pumpkin.h
      end
    end
  end
end

function hero_animate_legs(sprite)
  if (sprite.walkclock>0) then
    sprite.walkclock-=1
  else
    sprite.walkclock=4
    sprite.walkframe+=1
    if (sprite.walkframe>=4) then
      sprite.walkframe=0
    end
  end
end

function hero_update(sprite)
  deadtime=0

  -- walk
  if (btn(0)) then 
    sprite.faceright=false
    sprite_move(sprite,-hwvv[hwvp],0)
    if (hwvp<#hwvv) hwvp+=1
    hero_animate_legs(sprite)
  elseif (btn(1)) then 
    sprite.faceright=true
    sprite_move(sprite,hwvv[hwvp],0)
    if (hwvp<#hwvv) hwvp+=1
    hero_animate_legs(sprite)
  else
    sprite.walkframe=0
    sprite.walkclock=0
    if (hwvp>1) then
      if (sprite.faceright) then
        sprite_move(sprite,hwvv[hwvp],0)
      else
        sprite_move(sprite,-hwvv[hwvp],0)
      end
      hwvp-=1
    end
  end
  
  -- jump or gravity
  -- there's btnp() but it auto-repeats
  if (btn(4) and not btn4pv and (sprite.facey>0)) then
    hero_jump_down(sprite)
  end
  if (btn(4) and (hjvp<#hjvv)) then
    if (hjvp==1) then
      sfx(0)
    end
    sprite_move(sprite,0,-hjvv[hjvp])
    hjvp+=1
  else
    if (sprite_move(sprite,0,2)) then
      hjvp=#hjvv
    elseif (not btn(4)) then
      hjvp=1
    end
  end
  btn4pv=btn(4)
  
  -- face up/down/level
  if (btn(2)) then
    sprite.facey=-1
  elseif (btn(3)) then
    sprite.facey=1
  else 
    sprite.facey=0
  end
  
  -- pickup/toss
  if (btn(5) and not btn5pv) then
    hero_pickup_or_toss(sprite)
  end
  btn5pv=btn(5)
  
  -- update pumpkin
  if (sprite.pumpkin) then
    sprite.pumpkin.x=sprite.x+sprite.w/2-sprite.pumpkin.w/2
    sprite.pumpkin.y=sprite.y
  end
end

function hero_draw(sprite)
  local headtile=16
  if (sprite.facey<0) then
    headtile=20
  elseif (sprite.facey>0) then
    headtile=21
  end
  local dsty=sprite.y+16
  if (sprite.pumpkin) then
    dsty+=sprite.pumpkin.h
  end
  spr(
    headtile,
    sprite.x,
    dsty-2,1,1,
    sprite.faceright
  )
  local bodytile=0x11
  if (sprite.walkframe==1) then
    bodytile=0x16
  elseif (sprite.walkframe==2) then
    bodytile=0x17
  elseif (sprite.walkframe==3) then
    bodytile=0x18
  end
  spr(
    bodytile,
    sprite.x,
    dsty+6,1,1,
    sprite.faceright
  )
  if (sprite.armsup) then
    spr(
      19,
      sprite.x,
      dsty-1,1,1,
      sprite.faceright
    )
  else
    spr(
      18,
      sprite.x,
      dsty+6,
      1,1,
      sprite.faceright
    )
  end
end

function hero_kill(sprite)
  if (sprite.pumpkin) then
    toss_all(sprite,sprite.pumpkin,0)
  end
end





function sprite_update(sprite)
  if ((sprite.xiner<-16) or (sprite.xiner>16)) then
    sprite_move(sprite,0,-1)
  elseif (sprite.solid) then
    sprite_move(sprite,0,2)
  end
  if (sprite.xiner<-10) then
    sprite_move(sprite,-2,0)
    sprite.xiner+=2
  elseif (sprite.xiner<0) then
    sprite_move(sprite,-1,0)
    sprite.xiner+=1
  elseif (sprite.xiner>10) then
    sprite_move(sprite,2,0)
    sprite.xiner-=2
  elseif (sprite.xiner>0) then
    sprite_move(sprite,1,0)
    sprite.xiner-=1
  end
end

function explode_draw(sprite)
  local f=flr(sprite.clock/4)%6
  if (f>=3) then
    sprite.tileid=0x33-(f-3)
  else
    sprite.tileid=0x30+f
  end
  for i=0,9 do
    local dx=cos(i/10)*sprite.clock
    local dy=sin(i/10)*sprite.clock
    spr(sprite.tileid,
      sprite.x+dx-4,
      sprite.y+dy-4+16
    )
  end
end

function explode_update(sprite)
  sprite.clock+=1
  if (sprite.clock>120) then
    del(sprites,sprite)
  end
end

function explode(sprite)
  sfx(6)
  local e=mksprite(0,0)
  add(sprites,e)
  e.x=sprite.x+sprite.w/2
  e.y=sprite.y+sprite.h/2
  e.clock=0
  e.draw=explode_draw
  e.update=explode_update
  if (sprite.onkill) sprite.onkill(sprite)
end

function fire_update(sprite)
  sprite.animclock+=1
  if (sprite.animclock>=5) then
    sprite.animclock=0
    if (sprite.tileid==0x22) then
      sprite.tileid=0x23
    else
      sprite.tileid=0x22
    end
  end
  for victim in all(sprites) do
    if (
      victim.fragile and
      (victim.x<sprite.x+sprite.w) and
      (victim.y<sprite.y+sprite.h) and
      (victim.x+victim.w>sprite.x) and
      (victim.y+victim.h>sprite.y)
    ) then
      del(sprites,victim)
      explode(victim)
      if (victim.human) then
        deadbaby=true
      end
    end
  end
end

function susie_update(sprite)
  sprite_update(sprite)
  sprite.animclock+=1
  if (sprite.animclock>6) then
    sprite.animclock=0
    sprite.animframe+=1
    if (sprite.animframe>=4) sprite.animframe=0
    if (sprite.animframe==0) then
      sprite.tileid=0x24
    elseif (sprite.animframe==1) then
      sprite.tileid=0x25
    elseif (sprite.animframe==2) then
      sprite.tileid=0x24
    elseif (sprite.animframe==3) then
      sprite.tileid=0x26
    end
  end
  local walkok=false
  if (sprite.faceright) then
    walkok=sprite_move(sprite,1,0)
  else
    walkok=sprite_move(sprite,-1,0)
  end
  if (not walkok) then
    sprite.faceright=not sprite.faceright
  end
end





function sprite_victorious(sprite)
  local x=sprite.x
  local y=sprite.y+sprite.h
  local w=sprite.w
  local h=1
  if (map_flag_in_rect(x,y,w,h,2)) then
    return true
  end
  for other in all(sprites) do
    if (
      other.solid and
      (other.x<x+w) and
      (other.y<y+h) and
      (other.x+other.w>x) and
      (other.y+other.h>y) and
      sprite_victorious(other)
    ) then
      return true
    end
  end
  return false
end

function all_sprites_victorious()
  for sprite in all(sprites) do
    if (
      sprite.human and
      not sprite_victorious(sprite)
    ) then
      return false
    end
  end
  return true
end

function check_victory()
  deadtime+=1 -- hero resets if alive
  if (deadtime>60) then
    levelwrap=true
    deadbaby=true
    losscount+=1
    music(2)
    sfx(4)
    return
  end
  if (all_sprites_victorious()) then
    victorytime+=1
    if (victorytime>=20) then
      if (deadbaby) then
        losscount+=1
        sfx(4)
      else
        sfx(5)
      end
      levelwrap=true
      music(2)
    end
  else
    victorytime=0
  end
end

function world_update()
  clocks_tick()
  for sprite in all(sprites) do
    if (sprite.update) then
      sprite.update(sprite)
    end
  end
  check_victory()
end



function draw_world()
  map(0,0,0,16,16,12)  
  for sprite in all(sprites) do
    if (sprite.draw) then
      sprite.draw(sprite)
    else
      spr(
        sprite.tileid,
        sprite.x,
        sprite.y+16,
        1,1,sprite.faceright,
        sprite.carried
      )
    end
  end
end

function draw_gameover()
  rectfill(0,16,128,112,1)
  print("game over",47,30,7)
  print(reprtime(tt_f,tt_s,tt_m)..", "..losscount.." losses",35,40)
  print("thanks for playing!",24,66,6)
  print("press jump to play again",16,72,6)
end

function draw_levelwrap()
  if (deadbaby) then
    rectfill(0,16,128,112,8)
    print("failure!",50,60,7)
  else
    rectfill(0,16,128,112,3)
    print("success!",50,35,7)
    print("level time: "..reprtime(lt_f,lt_s,lt_m),29,58,6)
    print("    record: "..reprtime(lr_f,lr_s,lr_m),29,64,6)
    print("total so far: "..reprtime(tt_f,tt_s,tt_m),21,74,6)
    print("      record: "..reprtime(tr_f,tr_s,tr_m),21,80,6)
    print("losses: "..losscount,45,90,6)
  end
end

function reprtime(f,s,m)
  if (s<10) s="0"..s
  if (m<10) m="0"..m
  return m..":"..s
end

function draw_chrome()
  rectfill(0,0,128,16,0)
  rectfill(0,112,128,128,0)
  if (mapid>0) then
    print("l: "..reprtime(lt_f,lt_s,lt_m),0,113,4)
    print(reprtime(lr_f,lr_s,lr_m),64,113,9)
  end
  print("t: "..reprtime(tt_f,tt_s,tt_m),0,119,4)
  print(reprtime(tr_f,tr_s,tr_m),64,119,9)
  print("level "..mapid,0,11,4)
end




__gfx__
cccccccc3333333333333333cccccccc888888867755775577557755000000000000000000000000000000000000000000000000000000000000000000000000
cc8c8ccc3333333333333333cccccccc888888867755775577557755000000000000000000000000000000000000000000000000000000000000000000000000
ccc8cccc333333333c3c3c3ccccccccc888888865577557755775577000000000000000000000000000000000000000000000000000000000000000000000000
cc8c8ccc33333333cccccccccccccccc666666665c7c5c7c55775577000000000000000000000000000000000000000000000000000000000000000000000000
cccccccc33333333cccccccccccccccc88868888cccccccc77557755000000000000000000000000000000000000000000000000000000000000000000000000
cccccccc33333333cccccccccccccccc88868888cccccccc77557755000000000000000000000000000000000000000000000000000000000000000000000000
cccccccc33333333cccccccccccccccc88868888cccccccc55775577000000000000000000000000000000000000000000000000000000000000000000000000
cccccccc33333333cccccccccccccccc66666666cccccccc55775577000000000000000000000000000000000000000000000000000000000000000000000000
000000000006f600000606000f00000f00000000000000000006f6000006f6000006f60000000000000000000000000000000000000000000000000000000000
00000000000666000060006006000006000000000000000000066600000666000006660000000000000000000000000000000000000000000000000000000000
00444400000666000660006600000006004444000044440000066600000666000006660000000000000000000000000000000000000000000000000000000000
017f1740000666000600000600000060011f1140077f774000066600000666000006660000000000000000000000000000000000000000000000000000000000
017f1740000111000f00000f00000060077f7740011f114000011100001111000001110000000000000000000000000000000000000000000000000000000000
0fffff400001010000000000000000600fffff400fffff4000100100001000100001000000000000000000000000000000000000000000000000000000000000
0f22ff400001010000000000000000600f22ff400f22ff4001000010000100100000110000000000000000000000000000000000000000000000000000000000
00ffff4000010100000000000000060000ffff4000ffff4000000010000000010000100000000000000000000000000000000000000000000000000000000000
00833300004444000008000000008000009999000099990000999900009939000367670000000000000000000000000000000000000000000000000000000000
088883300441414080080800000880000ffff9900ffff9900ffff99009a939903377677000000000000000000000000000000000000000000000000000000000
88888833044444408008880808088800ff1fff90ff1fff90ff1fff909a9999996776777000000000000000000000000000000000000000000000000000000000
888888830045540088888888088888080fffff900fffff900fffff909a9999997767776700000000000000000000000000000000000000000000000000000000
8888888800144100888898888889888800fffe9000eefe9000ffee90999999996677776700000000000000000000000000000000000000000000000000000000
888888880111111008989888888989880eeeeee00eeeeee00eeeeeee999999997777767700000000000000000000000000000000000000000000000000000000
08888880044994400089898888889880eeeeeeeeeeeeeee00eeeeeee099999900776677600000000000000000000000000000000000000000000000000000000
00888800044994400089988008899800eeeeeeee0000eeeeeeeee000009999000007776700000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000070000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000700000070700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00070000007070000000000007000700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000700000070700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000070000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
__label__
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
33333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333
33333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333
33333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333
33333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333
33333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333
33333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333
33333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333333333333333333333333333cccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333333333333333333333333333cccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333333333333333333333333333cccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333333333333333333333333333cccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333333333333333333333333333cccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333333333333333333333333333cccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333333333333333333333333333cccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333333333333333333333333333cccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333333333333333333333333333cccccccccc4444cccccccccccc4444cccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333333333333333333333333333ccccccccc441414cccccccccc441414ccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
333333333c3c3c3c3c3c3c3c3c3c3c3cccccccccc444444cccccccccc444444ccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccc4554cccccccccccc4554cccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccc1441cccccccccccc1441cccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333ccccccccccccccccccccccccccccccccc111111cccccccccc111111ccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333ccccccccccccccccccccccccccccccccc449944cccccccccc449944ccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333ccccccccccccccccccccccccccccccccc449944cccccccccc449944ccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccc3333333333333333333333333333333333333333cccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccc3333333333333333333333333333333333333333cccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccc3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3ccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8333cccccccccccccccccccccccccccccccccc33333333
33333333ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc888833ccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc88888833cccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc88888883cccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc88888888cccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc88888888cccccccccccccccccccccccccccccccc33333333
33333333ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc888888ccccccccccccccccccccccccccccccccc33333333
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8888cccccccccccccccccccccccccccccccccc33333333
33333333cccccccc3333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333338888888688888886
33333333cccccccc3333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333338888888688888886
33333333cccccccc3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c8888888688888886
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc6666666666666666
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc6666666666666666
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8888888688888886
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8888888688888886
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8888888688888886
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc6666666666666666
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc6666666666666666
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccc3333333333333333333333333333333333333333cccccccc8888888688888886
33333333cccccccccccccccccccccccccccccccccccccccccccccccccccccccc3333333333333333333333333333333333333333cccccccc8888888688888886
33333333ccccccccccccccccccccccccccccccccccccccccccccc4444ccccccc3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3ccccccccc8888888688888886
33333333cccccccccccccccccccccccccccccccccccccccccccc471f71cccccccccccccccccccccccccccccccccccccccccccccccccccccc6666666666666666
33333333cccccccccccccccccccccccccccccccccccccccccccc471f71cccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccc4fffffcccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccc4ff22fcccccccccccccccccccccccccccccccccccccccccccccccccccccc8886888888868888
33333333cccccccccccccccccccccccccccccccccccccccccccc4ffffccccccccccccccccccccccccccccccccccccccccccccccccccccccc6666666666666666
3333333333333333ccccccccccccccccccccccccccccccccccccc6f6cccccccccccccccccccccccccccccccccccccccccccccccc888888868888888688888886
3333333333333333cccccccccccccccccccccccccccccccccccc66666ccccccccccccccccccccccccccccccccccccccccccccccc888888868888888688888886
3333333333333333ccccccccccccccccccccccccccccccccccc6666666cccccccccccccccccccccccccccccccccccccccccccccc888888868888888688888886
3333333333333333ccccccccccccccccccccccccccccccccccc6c666c6cccccccccccccccccccccccccccccccccccccccccccccc666666666666666666666666
3333333333333333cccccccccccccccccccccccccccccccccccfc111cfcccccccccccccccccccccccccccccccccccccccccccccc888688888886888888868888
3333333333333333ccccccccccccccccccccccccccccccccccccc1c1cccccccccccccccccccccccccccccccccccccccccccccccc888688888886888888868888
3333333333333333ccccccccccccccccccccccccccccccccccccc1c1cccccccccccccccccccccccccccccccccccccccccccccccc888688888886888888868888
3333333333333333ccccccccccccccccccccccccccccccccccccc1c1cccccccccccccccccccccccccccccccccccccccccccccccc666666666666666666666666
3333333333333333cccccccccccccccccccccccc333333333333333333333333333333333333333333333333cccccccccccccccc888888868888888688888886
3333333333333333cccccccccccccccccccccccc333333333333333333333333333333333333333333333333cccccccccccccccc888888868888888688888886
3333333333333333cccccccccccccccccccccccc333333333333333333333333333333333333333333333333cccccccccccccccc888888868888888688888886
3333333333333333cccccccccccccccccccccccc333333333333333333333333333333333333333333333333cccccccccccccccc666666666666666666666666
3333333333333333cccccccccccccccccccccccc333333333333333333333333333333333333333333333333cccccccccccccccc888688888886888888868888
3333333333333333cccccccccccccccccccccccc333333333333333333333333333333333333333333333333cccccccccccccccc888688888886888888868888
3333333333333333cccccccccccccccccccccccc333333333333333333333333333333333333333333333333cccccccccccccccc888688888886888888868888
3333333333333333cccccccccccccccccccccccc333333333333333333333333333333333333333333333333cccccccccccccccc666666666666666666666666
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

__gff__
0001020001060500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
__map__
0000000000000000000000000000000001010101010101010101010101010101040404040404040404040404040404040103030303030303030303030303030103030303030303030303030303030303040404040404040404040404040404040303030303030303030303030303030303030303030303030303030303030303
0000000000000000000000000000000001030303030303030301010101030301040303030303030303030303030303040103030303030303030303030303030103030303030303030303030303030303040303030303030303030303030303040303030303030303030303030303030303030303030303030303030303030303
0000000000000000000000000000000001030303030303030303030303030301040303030303030303030303030303040103030303030303030303030303030103030303030303030303030303030303040303030303030303030303030303040303030303030303030303030303030303030303030303030303030303030303
0000000000000000000000000000000001032103030303030303030303030301040303030303030303030303030303040103030303030303030303030303030103030303030303030303030303030303040505050505050505050505050505040303030303030303030306060606060603030303030303040404030303030303
0000000000000000000000000000000001020202030321030303030303240301040303030303030303030303030303040103030303030303030303030303030103100303210324030324030324032103040303030303030303030324242424040303030303030303030301010101010103030303030303040404030303030303
0000000000000000000000000000000001030303020202020203030305050501040303030303030305050505030303040110030303030303030505050505050102020202020202020202020202020202040303030303030302020202020202042827030303030303030301010101010103030303030303040404030303030303
0000000000000000000000000000000001030322030303200303030303030301040303030303030303030303030303040102030303030303210303030303030103030303030303030303030303030303040303030303030303030303030320040202020303030303030301010101010103030303030303040404030303030303
0000000000000000000000000000000001030202020202020202020202020404040303030303030303030303030303040103030303030302020203030303030102020203030303030303030303020202040202020202020202030303030320040303030303030303030301010101010121211003030303040404030303030303
0000000000000000000000000000000001030303030303030303030303030404040303030303030303030303030303040103030303030303030303030303030103030303030303030303030303030303040303030303030303030303030320040303030202020303030301010101010101010101030303040404030505050503
0000000000000000000000000000000001030303030303030202020202030404040303030303030303030303030303040103030303030303210303032403240103030303030303030303030303030303040303030303030303030303020202042028030303030303030301010101010101010101272820040404030303030303
0000000000000000000000000000000001010303030303031003030303040404040321031003032003032003202003040122220303030101010101010101010122030305050505050505050505030322040322031003030303030303030303040202020303030303030301010101010101010101010101040404222222222222
0000000000000000000000000000000001010303030101010101010303040404040404040404040404040404040404040101010101010101010101010101010104040404040404040404040404040404040404040404040404040404040404040303030303210310030301010101010101010101010101040404040404040404
0404040404040404040404040404040403030303030303030303030303030303010101010101010103030303030303010103030303030303030303030303030100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403030303030303030303030303240403030303030303030303030303030303010101010101010103030303030303010103030303030303030303030303030100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403030404040404040404040404040403030303030303030303030303030303010101010101010103030303030303010103030303030303030303030303030100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403030303030303030303030303030403030303030303030303030303030303010101010101010103030303030303010103030303030303030303030303030100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0404040404040404040404040403030403100303030303030303030303030303010101010101010103030303030303010103030303030303030303030303030100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403030303030303030303030303030401010103030303030303030303060606010324032403010103030303030303010110030303030303030305050505050100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403030303030303030303030303030401010103030303030303030303010101010202020202010103050505050503010102030303030303210303030303030100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403031003030303030303030303030401010103030303030303030303010101010303030303030303030303030303010103030303030302020203030303030100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403050505030303030303032203030401010103030303030303030303010101010303030303030303030303030303010103030303030303030303030303030100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403030303030303030304040404040401010103030303032003030303010101010303030303010102020202020303010103030303030303030303032424240100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0403030303032827282804040404040401010103030303272727030303010101010303100303010103030303030303010122222222010101010101010101010100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
0404040404040404040404040404040401010122030328282128280322010101010320272803010122222222222222010101010101010101010101010101010100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
__sfx__
011500002754124500005010050100501005010050100501005010050100501005010050100501005010050100501005010050100501005010050100501005010050100501005010000100000000000000000000
011000002c55300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
011000002403100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
011000001c55300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
001000001a15015130111200e14000100001000010000100001000010000100001000010000100001000010000100001000010000100001000010000100001000010000100001000010000100001000010000100
000b0000155400050000500155301a550005000050000500005000050000500005000050000500005000050000500005000050000500005000050000500005000050000500005000050000500005000050000500
3a1000002075300703007030070300703007030070300703007030070300703007030070300703007030070300703007030070300703007030070300703007030070300703007030070300703007030070300703
011900201a750000001c7301e740217500000000000237002375021700217401e7001e7500000000000000001a750000001c7301e74021750000002375000000217500000000000000000870006700077000a700
001900201a750000001c7301e740217500000000000237002375021700217401e7001e7500000000000000001a750000001c7301e740217500000015750000001a7500000000000000000870006700077000a700
001900200e0400000000000000000904000000000000e0000e0400000009030000000b0300b0000d030000000e04000000000000000009040000000e000000000d04000000090000000009040000000d00000000
001900200e0400000000000000000904000000000000e0000e0400000009030000000b0300b0000d030000000e04000000000000000009040000000e03000000020400000009020000000b020000000d02000000
__music__
01 07094344
02 080a4344
04 41424344

