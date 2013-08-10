probes = {}

function probe (p)
   if p[1] and p[2] then
      pprobe = {name=p[1],
		freq=p[2],
		handlers={},
		buf={},
		mt={}}
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
   if type(tuples) == "table" then
      for k, v in pairs(tuples) do
	 local b = probes[k].buf
	 -- add the value to the probe buffer
	 table.insert(b, 1, v)
	 b[b.maxlen+1] = nil
	 -- collect handlers to run in a "set" since we do not want
	 -- the same handler to run multiple times
	 if probes[k].handlers then
	    for _, h in pairs(probes[k].handlers)
	    do handlerset[h] = true end
	 end
      end

      -- execute all the handlers in separate coroutines
      for fn, flag in pairs(handlerset) do
	 -- fail silently
	 if flag then pcall(fn) end
      end
   end
end
