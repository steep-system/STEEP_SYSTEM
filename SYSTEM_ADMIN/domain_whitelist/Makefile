OUTPUT_NAME=domain_whitelist# modify name here for binary excutable file name
HARDLNK_LIBS=../lib/common.so ../lib/webkit_lib.so#add here the hard link libraries here
LIBRARY_PATHS=# add library paths here
LIBRARY_FILES=# add library files here
#object files under "lib" directory, used for linking
REMOTE_LINKS=../../common/common.so ../../webkit_lib/webkit_lib.so
LINK_FLAGS=$(patsubst %,-L%,$(LIBRARY_PATHS)) $(patsubst %,-l%,$(LIBRARY_FILES))
SCRIPTS_PATH=../scripts

.PHONY : normal
.PHONY : debug
.PHONY : link
.PHONY : clean
.PHONY : $(OUTPUT_NAME)
	
normal : $(OUTPUT_NAME)
	@$(SCRIPTS_PATH)/environment lib $(REMOTE_LINKS)
	@$(SCRIPTS_PATH)/compile normal
	$(SCRIPTS_PATH)/linker $(OUTPUT_NAME) $(LINK_FLAGS) $(HARDLNK_LIBS)
debug : $(OUTPUT_NAME)
	@$(SCRIPTS_PATH)/environment lib $(REMOTE_LINKS)
	@$(SCRIPTS_PATH)/compile debug
	$(SCRIPTS_PATH)/linker $(OUTPUT_NAME) $(LINK_FLAGS) $(HARDLNK_LIBS)
link : $(OUTPUT_NAME)
	$(SCRIPTS_PATH)/linker $(OUTPUT_NAME) $(LINK_FLAGS) $(HARDLNK_LIBS)
clean :
	@$(SCRIPTS_PATH)/compile clean
	-@rm $(OUTPUT_NAME) 1>/dev/null 2>&1
