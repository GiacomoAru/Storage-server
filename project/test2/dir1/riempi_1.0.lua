
local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));



local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));






local tArgs = {...}

local posiz = vector.new(0,0,0);
local verso = vector.new(0,1,0); 
local zero = vector.new(0,0,0);
local vx = vector.new(1,0,0);
local vy = vector.new(0,1,0);
local vup = vector.new(0,0,1); --orario
local vdown = vector.new(0,0,-1); --anti orario
local homeDist = 0;
local casa = vector.new(-13,-20,0);

local stato = {}

local isBBlock = function(a)
	return a.name == "minecraft:sand" or a.name == "minecraft:cobblestone" or 
		a.name == "minecraft:netherrack" or a.name == "minecraft:gravel" or a.name == "minecraft:stone"
    or a.name == "minecraft:stone_bricks";
end	

local move = function(where)
	
	local a , b = false, nil;
	if where == "forward" then 
		a,b = turtle.forward();
		if a then posiz = posiz:add(verso) end
	elseif where == "back" then 
		a,b = turtle.back();
		if a then posiz = posiz:sub(verso) end
	elseif where == "up" then 
		a,b = turtle.up();
		if a then posiz = posiz:add(vup) end
	elseif where == "down" then 
		a,b = turtle.down();
		if a then posiz = posiz:add(vdown) end
	end
	
	homeDist = posiz:dot(vx) + posiz:dot(vy);
	return a,b;
end

local dig = function(where)

	if where == "forward" then return turtle.dig()
	elseif where == "up" then return turtle.digUp()
	elseif where == "down" then return turtle.digDown() 
	end
	
end
local place = function(where)
	if where == "forward" then return turtle.place()
	elseif where == "up" then return turtle.placeUp()
	elseif where == "down" then return turtle.placeDown() 
	end
end
--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	if where == "forward" then 
		if turtle.detect() then dig("forward") end; 
	elseif where == "up" then 
		if turtle.detectUp() then dig("up") end; 
	elseif where == "down" then 
		if turtle.detectDown() then dig("down") end;
	end
	return move(where);
end 
local rotate = function(where)
	local a, a2,b = false, false, nil;
	
	if where == "left" then 
		a,b = turtle.turnLeft();
		if a then verso = verso:cross(vdown); end
	elseif where == "right" then 
		a,b = turtle.turnRight();
		if a then verso = verso:cross(vup); end
	elseif where == "back" then 
		a , b = turtle.turnRight();
		if a then a2, b = turtle.turnRight(); end
		if a and a2 then verso = verso:unm(); end
		if a and not a2 then turtle.turnLeft(); end
		a = a and a2;
	end
	return a,b;
end
local absRotate = function(vwhere) --vettore 
	if vup:dot(verso:cross(vwhere)) > 0 then return rotate("left") 
	elseif vup:dot(verso:cross(vwhere)) < 0 then return rotate("right")
	elseif verso:dot(vwhere) < 0 then return rotate("back") end
end
local goTo = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
end
local goToInv = function(dest)--si muove verso il punto di riferimento, prima z poi y poi x
	local diff = dest:sub(posiz);
	local x = diff:dot(vx);
	local y = diff:dot(vy);
	local z = diff:dot(vup);
	
	local i = math.abs(x);
	while i > 0 do
	
		absRotate(vx);
		if x > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(y);
	while i > 0 do
		
		absRotate(vy);
		if y > 0 then
			move("forward");
		else
			move("back");
		end
		i = i - 1;
	end
	i = math.abs(z);
	while i > 0 do
		
		if z > 0 then
			move("up");
		else
			move("down");
		end
		i = i - 1;
	end
end

local reset = function()
	local posizioneIniz = posiz;
	local versoIniz = verso;
	goTo(casa);
	absRotate(vx:unm());
	turtle.select(1);
	turtle.drop();
	turtle.suck();
	
	local i = 2;
	while i < 17 do
		turtle.select(i);
		local dummy = turtle.getItemCount(i);
		if turtle.getItemCount(i) > 0 and not isBBlock(turtle.getItemDetail(i)) then 
			dummy = 0;
			turtle.dropDown();
		end
		turtle.suckUp(64 - dummy);
		i = i + 1;
	end
	
	if not redstone.getInput("left") then 
		print("STOPPATEEH!\n" .. "stato attuale: " .. textutils.serialize(stato));
		os.queueEvent("terminate");
	end
	goToInv(posizioneIniz);
	absRotate(versoIniz);
end

local cFMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return move(where);
end
local cFDigAndMove = function(where)
	local cond = true;
	if turtle.getFuelLevel() < 1 + homeDist then 
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	return digAndMove(where);
end
local cFGoTo = function(dest, a)
	local diff = dest:sub(posiz);
	if turtle.getFuelLevel() < diff:dot(vx) + diff:dot(vy) + diff:dot(vup) + homeDist  then
		turtle.select(1);
		cond = turtle.refuel();
	end
	if not cond then reset() end
	
	if a then return goTo(dest) else goToInv(dest)end
end

local checkBBlock = function(ss)
	local ret = false;
	while not ret and ss <= 16 do
		if turtle.getItemCount(ss) > 0 and isBBlock(turtle.getItemDetail(ss)) then ret = true --da modificare??
		else ss = ss + 1;
		end
	end
	
	if ret then return ret, ss;
	else return ret, nil; end
end


local start = function(avanti, destra)
	local da, dd = 0,0 ;
	local selectedS = 2;
	reset();
	
	while dd < destra do
		while da < avanti do
			stato[1] = da;
			stato[2] = dd;
			
			
			local depth = 0;
			local a, b = turtle.inspectDown();
			while a == false or not isBBlock(b) do
				cFDigAndMove("down");
				a, b = turtle.inspectDown();
				depth = depth + 1;
			end
			
			while depth > 0 do
				cFMove("up");
				local ifTI, whereI = checkBBlock(selectedS);
				if not ifTI then 
					reset();
					selectedS = 2;
					ifTI, whereI = checkBBlock(selectedS);
					if not ifTI then 
						print("avanti fatto: " .. da .. "\n destra fatto: " .. dd .."\n PROBLEMA SCONOSCIUTO");
						os.queueEvent("terminate");
					end
				end
				selectedS = whereI;
				turtle.select(selectedS);
				
				place("down");
				depth = depth - 1;
			end
			
			
			
			da = da + 1;
			if da < avanti then cFDigAndMove("forward"); end
		end
		if dd % 2 == 0 then 
			rotate("right");
			cFDigAndMove("forward");
			rotate("right");
		else 
			rotate("left");
			cFDigAndMove("forward");
			rotate("left");
		end
			
		da = 0;
		dd = dd + 1;
	end
	
	print("avanti fatto: " .. da .. "\n destra fatto: " .. dd);
end

start(tonumber(tArgs[1]), tonumber(tArgs[2]));








