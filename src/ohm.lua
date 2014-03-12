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
   if type(tuples) ~= "table" then return end
   for k, v in pairs(tuples) do
      local b = probes[k].buf
      -- add the value to the probe buffer only if it is different
      if b[1] and type(b[1]) == "table" then
	 local eq = false
	 for x, y in pairs(b[1]) do
	    if v[x] ~= y then eq = true break end
	 end
	 if not eq then break end
      elseif b[1] and b[1] == v then break end
      table.insert(b, 1, v)
      b[b.maxlen+1] = nil
      -- collect handlers to run in a "set" since we do not want
      -- the same handler to run multiple times
      if not probes[k].handlers then break end
      for _, h in pairs(probes[k].handlers) do
	 if not handlerset[h] then
	    -- execute all the handlers in separate coroutines
	    local thd = coroutine.create(h)
	    coroutine.resume(thd)
	    handlerset[h] = true
	 end
      end
   end
end
