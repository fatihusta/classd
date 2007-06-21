#FIXME: make this better !!!

release: force
	lftp -e "cd incoming ; put `find . -name *deb | xargs`" mephisto

force:
