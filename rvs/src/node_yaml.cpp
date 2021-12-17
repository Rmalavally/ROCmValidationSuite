#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include "include/node_yaml.h"
/* Set environment variable DEBUG=1 to enable debug output. */
int debug = 0;
std::string rvsmodulename;
CollectionMap collectionMap =
{
  { "gpup", {"properties", "io_links-properties"} },
  { "peqt" , {"capability",} },
  { "gm"   , {"metrics",} }
};

/* with device_id.sh script, chars added to every line of conf
* , so using this to temp cleanup
*/
std::string cleanModuleName(std::string modname){
  std::string str;
  if(rvsmodulename.find("gpup") != std::string::npos)
    str = "gpup";
  if(rvsmodulename.find("gst") != std::string::npos)
    str = "gst";
  if(rvsmodulename.find("iet") != std::string::npos)
    str = "iet";
  if(rvsmodulename.find("pebb") != std::string::npos)
    str = "pebb";
  if(rvsmodulename.find("peqt") != std::string::npos)
    str = "peqt";
  if(rvsmodulename.find("pesm") != std::string::npos)
    str = "pesm";
  if(rvsmodulename.find("pqt") != std::string::npos)
    str = "pqt";
  if(rvsmodulename.find("mem") != std::string::npos)
    str = "mem";
  return str;

}

bool isCollection(const std::string& property){
  if(rvsmodulename.empty()){
    return false;
  }
  if(collectionMap.find(rvsmodulename) == collectionMap.end()){
    return false;
  }
  auto cvec = collectionMap[rvsmodulename];
  if(std::find(cvec.begin(), cvec.end(), property) != cvec.end())
    return true;
  return false;
}

/*
 * Consume yaml events generated by the libyaml parser to
 * import our data into raw c++ data structures.
 */
int consume_event(std::shared_ptr<parser_state> s, yaml_event_t *event)
{
    char *value;
    std::string temp;
    std::string key;
    ActionMap f;
    bool collection = false;
    if (true) {
        printf("state=%d event=%d\n", s->state, event->type);
    }
    switch (s->state) {
    case STATE_START:
        switch (event->type) {
        case YAML_STREAM_START_EVENT:
            s->state = STATE_STREAM;
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

     case STATE_STREAM:
        switch (event->type) {
        case YAML_DOCUMENT_START_EVENT:
            s->state = STATE_DOCUMENT;
            break;
        case YAML_STREAM_END_EVENT:
            s->state = STATE_STOP;  /* All done. */
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

     case STATE_DOCUMENT:
        switch (event->type) {
        case YAML_MAPPING_START_EVENT:
            s->state = STATE_SECTION;
            break;
        case YAML_DOCUMENT_END_EVENT:
            s->state = STATE_STREAM;
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

    case STATE_SECTION:
        switch (event->type) {
        case YAML_SCALAR_EVENT:
            value = (char *)event->data.scalar.value;
            if (strcmp(value, ACTIONS.c_str()) == 0) {
               s->state = STATE_ACTIONLIST;
            } else {
               fprintf(stderr, "Unexpected scalar: %s\n", value);
               return FAILURE;
            }
            break;
        case YAML_DOCUMENT_END_EVENT:
            s->state = STATE_STREAM;
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

    case STATE_ACTIONLIST:
        switch (event->type) {
        case YAML_SEQUENCE_START_EVENT:
            s->state = STATE_ACTIONVALUES;
            break;
        case YAML_MAPPING_END_EVENT:
            s->state = STATE_SECTION;
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

    case STATE_ACTIONVALUES:
        switch (event->type) {
        case YAML_MAPPING_START_EVENT:
            s->state = STATE_ACTIONKEY;
            break;
        case YAML_SEQUENCE_END_EVENT:
            s->state = STATE_ACTIONLIST;
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

    case STATE_ACTIONKEY:
        switch (event->type) {
        case YAML_SCALAR_EVENT:
            // make a key and save state as value state
            key = (char *)event->data.scalar.value;
	    s->keyname = key;
	    collection = isCollection(key);
	    if(collection){
		std::cout << " got collecttion key " << key << std::endl;
		s->state = STATE_COLLECTION;
	    }else{
                s->state = STATE_ACTION_VALUE;
	    }
            break;
        case YAML_MAPPING_END_EVENT:
	    s->actionlist.push_back( s->f);
	    s->f.clear();
            s->state = STATE_ACTIONVALUES;
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

    case STATE_ACTION_VALUE:
        switch (event->type) {
        case YAML_SCALAR_EVENT:
            temp = (char *)event->data.scalar.value;
	          if(s->keyname.empty()){
                std::cout << "cant have empty key " << std::endl;
                return FAILURE;
            }
            s->f.emplace(s->keyname, std::string(temp));
	          s->keyname.clear();
            s->state = STATE_ACTIONKEY;
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

    case STATE_COLLECTION:
        switch (event->type) {
        case YAML_SEQUENCE_START_EVENT:
            s->state = STATE_COLLECTIONLIST;
            break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

   case STATE_COLLECTIONLIST:
        switch (event->type) {
        case YAML_MAPPING_START_EVENT:
            s->state = STATE_COLLECTIONKEY;
            break;
	      case YAML_SEQUENCE_END_EVENT:
	          s->state = STATE_ACTIONKEY;
	          break;
        default:
            fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
            return FAILURE;
        }
        break;

    case STATE_COLLECTIONKEY:
	       switch (event->type) {
         case YAML_SCALAR_EVENT:
	         value = (char *)event->data.scalar.value;
	         s->colkey = s->keyname + "." + std::string(value);
	         s->state = STATE_COLLECTIONVALUE;
	         break;
	       case YAML_MAPPING_END_EVENT:
	         s->state = STATE_COLLECTIONLIST;
	         break;
         default:
	         fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
           return FAILURE;
	       }
	       break;

    case STATE_COLLECTIONVALUE:
       switch (event->type) {
       case YAML_SCALAR_EVENT:
         temp = (char *)event->data.scalar.value;
         s->f.emplace(s->colkey, std::string(temp));
	       s->colkey.clear();
	       s->state = STATE_COLLECTIONKEY;
	       break;
	     default:
	       fprintf(stderr, "Unexpected event %d in state %d.\n", event->type, s->state);
	       return FAILURE;    
       }
       break;

    case STATE_STOP:
        break;
    }
    return SUCCESS;
}

int parse_config(std::shared_ptr<parser_state> state,std::string filename){
    int code;
    int status_;
    //struct parser_state state;
    //parser_state *state = new parser_state{};
    yaml_parser_t parser;
    yaml_event_t event;

    if (getenv("DEBUG")) {
        debug = 1;
    }
    FILE *fd = fopen(filename.c_str(), "r");
    if(!fd){
        std::cout << "Could not open file " << std::endl;
        return -1;
    }
    rvsmodulename = getModule(filename);
    rvsmodulename = cleanModuleName(rvsmodulename);
    state->state = STATE_START;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fd);
    do {
        status_ = yaml_parser_parse(&parser, &event);
        if (status_ == FAILURE) {
            fprintf(stderr, "yaml_parser_parse error\n");
            code = EXIT_FAILURE;
            goto done;
        }
        status_ = consume_event(state, &event);
        if (status_ == FAILURE) {
            fprintf(stderr, "consume_event error\n");
            code = EXIT_FAILURE;
            goto done;
        }
        yaml_event_delete(&event);
    } while (state->state != STATE_STOP);

    code = EXIT_SUCCESS;

done:
    //destroy_actions(state->actionlist);
    yaml_parser_delete(&parser);
    //delete state;
    return code;
}
