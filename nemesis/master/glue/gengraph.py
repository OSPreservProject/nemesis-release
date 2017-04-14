#
# gengraph; a quick hack to produce a VCG GDL file of the requires graph
#

import blueprint

items = blueprint.items

print 'graph: { title: "Nemesis Build System"'
confdb = {}
for item in items:
    print 'node: { title : "'+item.get_name()+'"}'
    confname = item.get_config_name()
    if confname:
	confdb[confname] = item

for item in items:
    l = item.get_requires_set()
    for req in l:
	if confdb.has_key(req):
	    target = confdb[req]
	    print 'edge: { sourcename: "'+item.get_name()+'" targetname: "'+target.get_name() +'"}'

print '}'
