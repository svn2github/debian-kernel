version := 0.0.1
date    := $(shell date)

all: version.ent
	debiandoc2html kernel-handbook.sgml
	
version.ent: 
	rm -f $@
	echo "<!entity version \"$(version)\">" >> $@
	echo "<!entity date    \"$(date)\">"    >> $@
