#!/usr/bin/env lua
require('strict')
require('util')
posix = require('posix')
lom = require('lxp.lom')

-- main
pid = posix.getpid().pid
sj_dir = assert(os.getenv('SJ_DIR'))
path_out = sj_dir .. '/in'
file_out = assert(io.open(path_out, 'w'))

s_out = string.format(
	[[<iq type='get' id='roster-%d'><query xmlns='jabber:iq:roster'/></iq>]],
	pid)
file_out:write(s_out)
file_out:close()

posix.sleep(1)

--pid = 777
path_in = sj_dir .. '/roster-' .. pid

file_in = assert(io.open(path_in, 'r'))
contents = assert(file_in:read('*a'))
assert(file_in:close())
-- assert(posix.unlink(path_in))

tab = lom.parse(contents)
--print(table.tostringFull(tab))
assert(tab.tag == 'iq' and tab[1].tag == 'query')
for i = 1, #tab[1] do
	assert(tab[1][i].tag == 'item')
	buddy = tab[1][i].attr
	jid = buddy.jid
	sub = buddy.subscription
	name = buddy.name or ""
	print(string.format('%-30s  %-6s  %s', jid, sub, name))
end
