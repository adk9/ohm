local inspect = require 'inspect'

probes = {}

function probe (p)
   if p[1] and p[2] then
      pprobe = {name=p[1], freq=p[2], handlers={}, buf={}, mt={}}
      setmetatable(pprobe, pprobe.mt)
      pprobe.mt.__index = function (table, key) return table.buf[key] end
      -- let's use a fixed window for now
      pprobe.buf.maxlen = 16
      probes[p[1]] = pprobe
   end
   return pprobe
end

function rule (r)
   -- check if all rule arguments are tables or not
   for _,v in pairs(r) do
      if type(v) ~= "table" then error("invalid probe in rule arguments") end
   end
   return function (h) for _,v in pairs(r) do table.insert(v.handlers, h[1]) end end
end

function ohm_add (tuples)
   local handlerset = {}
   -- if type(tuples) ~= "table" then return end
   for k, v in pairs(tuples) do
      local b = probes[k].buf
      -- add the value to the probe buffer only if it has changed.
      local skip = false
      if b[1] then
	 if type(b[1]) == "table" and type(v) == "table" then
            skip = true
	    for x, y in pairs(b[1]) do
	       if v[x] ~= y then skip = false break end
	    end
	 elseif b[1] == v then skip = true end
      end

      if not skip then
	 table.insert(b, 1, v)
	 b[b.maxlen+1] = nil
      end

      -- collect handlers to run in a "set" since we do not want
      -- the same handler to run multiple times
      if not probes[k].handlers then break end
      for _, h in pairs(probes[k].handlers) do
	 if not handlerset[h] then
	    handlerset[h] = true
	 end
      end
   end
   for h, _ in pairs(handlerset) do
      -- execute all the handlers in separate coroutines
      local thd = coroutine.create(h)
      coroutine.resume(thd)
   end
end
