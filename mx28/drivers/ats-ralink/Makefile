
all:
	$(MAKE) -C UTIL/ osutil
	$(SHELL) cp_util.sh
	$(MAKE) -C MODULE/ build_tools
	$(MAKE) -C MODULE/ osdrv
	$(SHELL) cp_module.sh
	$(MAKE) -C NETIF/ osnet

clean:
	$(MAKE) -C UTIL/ clean
	$(MAKE) -C MODULE/ clean
	$(MAKE) -C NETIF/ clean

install:
	$(MAKE) -C UTIL/ install
	$(MAKE) -C MODULE/ install
	$(MAKE) -C NETIF/ install

uninstall:
	$(MAKE) -C UTIL/ uninstall
	$(MAKE) -C MODULE/ uninstall
	$(MAKE) -C NETIF/ uninstall
