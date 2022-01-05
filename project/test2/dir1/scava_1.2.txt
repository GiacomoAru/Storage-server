
local tArgs = {...}

local passiTotali = 0;
local blocchiScavati = 0;
local mineraliScavati = 0;

local latoDaScavare = "right";
local pattern = 3;
local numeroDiTunnel = 32;
local profTunnel = 4;
local tunnelFatti = 0;

--per ora il numero di passi massimi supportati per scavare interamente una vena di materiale
local passiAgg = 150;

--risolve un problema irreparabile, spegne la turtle lol
local irreparableIssue = function(s) 
	print("Errore fatale: " .. s);
	os.shutdown(); 
end

--determina quali materiali sono da "buttare"
local isTrash = function(p)
	return p == "minecraft:cobblestone" or 
			p == "minecraft:diorite" or 
			p == "minecraft:granite" or 
			p == "minecraft:andesite"
end
--determina se è combustibile
local isFuel = function(p)
	return p == "minecraft:coal" or p == "minecraft:coal_block"
end
local isOre = function(p)
	return p.tags["forge:ores"]
end
--determina se un blocco ha la gravità (DA IMPLEMENTARE) lol
local haveGravity = function(p)
	local a,b = false, nil;
	
	if p == "forward" then a,b = turtle.inspect();
	elseif p == "up" then a,b = turtle.inspectUp();
	elseif p == "down" then a,b = turtle.inspectDown();
	end
	
	if(a) then return b.name == "minecraft:sand" or b.name == "minecraft:gravel" 
	end 
	
	return false
end

--funzioni base turtle
local move = function(where)
	if where == "forward" then turtle.forward()
	elseif where == "back" then turtle.back()
		elseif where == "up" then turtle.up()
			elseif where == "down" then turtle.down() end
	
	passiTotali = passiTotali + 1;
end
local dig = function(where)

	if where == "forward" then turtle.dig()
	elseif where == "up" then turtle.digUp()
	elseif where == "down" then turtle.digDown() 
	end
	
	blocchiScavati = 1 + blocchiScavati;
end
local intelligentDig = function(where)
	dig(where);
	while(haveGravity(where)) do 
		dig(where);
		blocchiScavati = 1 + blocchiScavati;
	end
end

--si muove e se serve scava, non scava se si muove indietro, non serve se usata bene
local digAndMove = function(where)
	
	if where == "forward" then if turtle.detect() then intelligentDig("forward") end; turtle.forward()
		elseif where == "up" then if turtle.detectUp() then intelligentDig("up") end; turtle.up()
			elseif where == "down" then if turtle.detectDown() then intelligentDig("down") end;turtle.down() end
	
	passiTotali = 1 + passiTotali;
end 
local rotate = function(where)
	if where == "left" then turtle.turnLeft()
	elseif where == "right" then turtle.turnRight()
	elseif where == "back" then turtle.turnRight(); turtle.turnRight(); end
end

--implementazione di una semplice pila, utilizzado una table
local crea = function() 
	return { lun = 0 ; pila = {} }
end	
local push = function(a,p)
	p.lun = p.lun + 1;
	p.pila[p.lun] = a;
end
local pop = function(p)	
	p.lun = p.lun - 1;
	return p.pila[p.lun + 1]
end
local inspectFirst = function(p)
	return p.pila[p.lun]
end
local isEmpty = function(p)
	return p.lun <= 0 
end


--mina tutto il filone di minerale
local checkAndMine = function()
	--1 trovato sopra
	--2 trovato sotto
	--3 trovato a sinistra
	--4 trovato dietro
	--5 trovato a destra
	--6 avanti
	pl = crea();
	attBlock = 1;
	repeat
		local ifb, whatb = false, nil;
		
		if(attBlock >= 7 and not(isEmpty(pl)) ) then
			attBlock = pop(pl);
			
			if attBlock == 1 then move("down") end
			if attBlock == 2 then move("up") end 
			if attBlock > 2 then move("back") end
			
			attBlock = 1 + attBlock;
		end
		
		if attBlock == 1 then
			ifb, whatb = turtle.inspectUp();
			if(ifb and isOre(whatb)) then
				digAndMove("up");
				push(attBlock, pl);
				attBlock = 0
			end
		elseif attBlock == 2 then
			ifb, whatb = turtle.inspectDown();
			if(ifb and isOre(whatb)) then
				digAndMove("down");
				push(attBlock, pl);
				attBlock = 0
			end	
		elseif attBlock <= 6 then 
			rotate("left");
			ifb, whatb = turtle.inspect();
			if(ifb and isOre(whatb)) then
				digAndMove("forward");
				push(attBlock, pl);
				attBlock = 0
			end
		end
		
		if(ifb and isOre(whatb)) then mineraliScavati = 1 + mineraliScavati end
		attBlock = 1 + attBlock;
	until isEmpty(pl) and attBlock >= 7
end
--serve per un controllo più efficente
local simpleCheck = function()

	local i = 0;
	local ifb, whatb = false, nil;
	
	ifb, whatb = turtle.inspectUp();
	if(ifb and whatb.tags["forge:ores"]) then
		return true
	end
	ifb, whatb = turtle.inspectDown();
	if(ifb and whatb.tags["forge:ores"]) then
		return true
	end
	ifb, whatb = turtle.inspect();
	if(ifb and whatb.tags["forge:ores"]) then
		return true
	end
	rotate("left");
	ifb, whatb = turtle.inspect();
	if(ifb and whatb.tags["forge:ores"]) then
		rotate("right");
		return true
	end
	rotate("left");
	rotate("left");
	ifb, whatb = turtle.inspect();
	if(ifb and whatb.tags["forge:ores"]) then
		rotate("left");
		return true
	end
	rotate("left");
	return false;
end

--resetta l'inventario, deve essere nella "home" della turtle
local reset = function() --sotto inventario sopra carbone destra cobblestone
	print("-----Sistemo l'inventario-----")
	rotate("right");
	
	turtle.select(1);
	turtle.dropUp();
	turtle.suckUp(32);
	if not isFuel(turtle.getItemDetail(1).name) then irreparableIssue("inventario"); end
	
	local i = 2;
	
	while i <= 16 do
		turtle.select(i);
		if turtle.getItemCount(i) > 0 and isTrash(turtle.getItemDetail(i).name) then 
			turtle.drop() else turtle.dropDown() end
		i = i + 1
	end
	
	rotate("left");
end

--controlla se abbiamo almeno uno slot vuoto
local checkEmptySpace = function() 
	local i = 1;
	local ret = false;
	
	repeat i = i + 1; ret = turtle.getItemCount(i) == 0 until (i < 16 or ret)
	return ret
end 
	
--se può e se serve ricarica il combustibile, in più controlla se siamo a corto di combustibile o siamo pieni, ritorna
--la coppia della valutazione dei problemi (OOF, IF)
local checkIssues = function(done)
	
	turtle.select(1);
	local b = turtle.getItemCount(1) - 1 <= 0;
	if( not b and turtle.getFuelLevel() < done + passiAgg) then turtle.refuel(turtle.getItemCount(1) - 1) end;
	
	local outOfFuel = turtle.getFuelLevel() < done + passiAgg;
	local inventoryFull = not checkEmptySpace;
	
	return outOfFuel, inventoryFull
end

--da cambiare se vogliamo fare alla nostra turtle un percorso non dritto
local continueStright = function(a)
	while a > 0 do
		digAndMove("forward");
		a = a - 1;
	end
end
local returnStright = function(a)
	if a <= 0 then return end
	
	rotate("back");
	while a > 0 do
		digAndMove("forward");
		a = a - 1;
	end
	rotate("back");
end

--mina diritto e non solo
local straightMine = function(passiAvanti, depth)
	local i = 0;
	
	print("Avanti tutta")
	continueStright(passiAvanti);
	
	while passiAvanti < depth do
		
		local a, b = checkIssues(passiAvanti);
		if (a or b) then
			print("Ritorno alla base, ho problemini")		
			returnStright(passiAvanti);
			
			return passiAvanti, a, b 
		end
		
		print("Scavo... :3                Fuel: " .. turtle.getFuelLevel())
		turtle.select(1);
		digAndMove("forward");
		
		passiAvanti = 1 + passiAvanti;
		if simpleCheck() then checkAndMine(); end
	end
	
	returnStright(passiAvanti);

	return passiAvanti, false, false
end

--va e torna dall'inizio del tunnel: lato deve essere uguale per goToSpot e goHome: il lato verso cui fare i tunnel
local goToSpot = function(fatti, pattern, lato)

	if pattern < 1 or pattern > 5 then 
		irreparableIssue("pattern di scavo");
	end 
	
	local mod = fatti % pattern;
	local coeff = math.floor(fatti/pattern) * 4;
	digAndMove("forward");
	
	while coeff > 0 do
		digAndMove("forward");
		coeff = coeff - 1;
	end 
	
	if pattern > 1 and mod == 1 then 
		digAndMove("up");
		digAndMove("up");
		digAndMove("forward");
		digAndMove("forward");
	end
	if pattern > 2 and mod == 2 then 
		digAndMove("down");
		digAndMove("down");
		digAndMove("forward");
		digAndMove("forward");
	end
	if pattern > 3 and mod == 3 then 
		digAndMove("up");
		digAndMove("up");
		digAndMove("up");
		digAndMove("up");
	end
	if pattern > 4 and mod == 4 then 
		digAndMove("down");
		digAndMove("down");
		digAndMove("down");
		digAndMove("down");
	end
	
	rotate(lato);
end

local goHome = function(fatti, pattern, lato)
	rotate(lato);
	
	if pattern < 1 or pattern > 5 then 
		irreparableIssue("pattern di scavo");
	end 
	
	local mod = fatti % pattern;
	local coeff = math.floor(fatti/pattern) * 4;
	
	if pattern > 1 and mod == 1 then 
		digAndMove("forward");
		digAndMove("forward");
		digAndMove("down");
		digAndMove("down");
	end
	if pattern > 2 and mod == 2 then 
		digAndMove("forward");
		digAndMove("forward");
		digAndMove("up");
		digAndMove("up");
	end
	if pattern > 3 and mod == 3 then 
		digAndMove("down");
		digAndMove("down");
		digAndMove("down");
		digAndMove("down");
	end
	if pattern > 4 and mod == 4 then 
		digAndMove("up");
		digAndMove("up");
		digAndMove("up");
		digAndMove("up");		
	end
	
	while coeff > 0 do
		digAndMove("forward");
		coeff = coeff - 1;
	end 
	
	digAndMove("forward");
	rotate("back");
end 

--iniziza gli scavi e gestisce il tutto
local start = function(pattern, nTunnel, dTunnel, lato, tunnelDone) 

	local passiFatti = 0;
	while tunnelDone < nTunnel and redstone.getInput("back") do
		
		local coeff = math.floor(tunnelDone/pattern) * 4;
		passiAgg = 5 + coeff + 100; --un caso pessimo
		reset();
		
		while passiFatti < dTunnel do
			goToSpot(tunnelDone, pattern, lato);
			
			local pf, i1, i2 = straightMine(passiFatti, dTunnel);
			passiFatti = pf;
			
			goHome(tunnelDone, pattern, lato);
			if i1 or i2 then reset(); end
		end
		
		tunnelDone = 1 + tunnelDone;
		passiFatti = 0;
	end
	
	reset();
	print("Scavo... :3                  Fuel: ")
	print("################FINE################")
	print("Tunnel scavati:           " .. tunnelDone)
	print("Lunghezza dei tunnel:     " .. dTunnel)
	print("Minerali totali scavati:  " .. mineraliScavati)
	print("Passi totali fatti:       " .. passiTotali)
	print("BRAVI TUTTI :3")
end 

if #tArgs > 0 then pattern = tonumber(tArgs[1]); numeroDiTunnel = pattern end
if #tArgs > 1 then profTunnel = tonumber(tArgs[2]) end
if #tArgs > 2 then latoDaScavare = tArgs[3] end
if #tArgs > 3 then numeroDiTunnel = tonumber(tArgs[4]) end
if #tArgs > 4 then tunnelFatti = tonumber(tArgs[5]) end

print(textutils.serialize(tArgs));
start(pattern, numeroDiTunnel, profTunnel, latoDaScavare, tunnelFatti)







