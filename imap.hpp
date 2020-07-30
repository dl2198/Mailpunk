#ifndef IMAP_H
#define IMAP_H
#include "imaputils.hpp"
#include <libetpan/libetpan.h>
#include <string>
#include <functional>

namespace IMAP {
  
  //forward declaration of Session class
  //necessary for member variable 'session' in Message
  class Session;

  class Message {
    
    int errorcode;
    std::string body;
    std::string field;
    uint32_t uid;

    //pointer to the session object the message belongs to
    Session* session;

    //represent a single message and its sections
    struct mailimap_set* single_message = nullptr;
    struct mailimap_section* section = nullptr;
    struct mailimap_header_list* header_list = nullptr;

    //list of fetch results
    clist* fetch_result = nullptr;

    //list of fetch requests
    struct mailimap_fetch_type* fetch_type = nullptr;

    //fetch request
    struct mailimap_fetch_att* fetch_att = nullptr;

    //function to free the memory used for fetching
    void free_fetch_memory();

    //list of flags
    struct mailimap_flag_list* flags = nullptr;

    //flag
    struct mailimap_flag* deleted = nullptr;

    //function to free the memory used for deleting a message
    void free_delete_memory();

    //change flags of messages
    struct mailimap_store_att_flags* store_att = nullptr;

    //function to get the data of the message
    std::string fetch_msg_data(struct mailimap_msg_att* msg_att);

  public:
    
    Message(){};

    //setter function for uid
    void set_uid(uint32_t uid);

    //getter function for uid
    uint32_t get_uid() {return uid;};

    //setter function for session
    void set_imap(Session* session);
    
    //get the body of the message
    std::string getBody();

    //get one of the descriptor fields
    std::string getField(std::string fieldname);

    //remove this mail from its mailbox
    void deleteFromMailbox();

  };

  class Session {
    
    int errorcode;

    //imap session
    mailimap* imap = nullptr;

    //login state of the session
    bool loggedin = false;

    //list of messages in the mailbox
    Message** messages = nullptr;

    //represent messages in a set
    struct mailimap_set* set = nullptr;

    //number of messages
    uint32_t message_no = 0;

    //function to get the number of messages
    //sets member attribute message_no
    void fetch_message_no();

    //function to free memory allocated in message list
    void free_messages();

    //list of fetch results
    clist* fetch_result = nullptr;

    //list of fetch requests
    struct mailimap_fetch_type* fetch_type = nullptr;

    //fetch request
    struct mailimap_fetch_att* fetch_att = nullptr;

    //status attributes requests
    struct mailimap_status_att_list* sa_list = nullptr;

    //status attribute fetch results
    struct mailimap_mailbox_data_status* sa_result = nullptr;

    //function to free memory used for status info
    void free_status_memory();

    //function to free memory used for fetching
    void free_fetch_memory();

    //mailbox
    std::string mb;

  public:

    Session(std::function<void()> updateUI);

    //Get all messages in the INBOX mailbox terminated by a nullptr
    Message** getMessages();

    //Connect to the specified server
    void connect(std::string const& server, size_t port = 143);

    //Log in to the server (connect first, then log in)
    void login(std::string const& userid, std::string const& password);

    //Select a mailbox (only one can be selected at any given time)
    void selectMailbox(std::string const& mailbox);

    //Get the uid value from the attribute list
    uint32_t fetch_uid(mailimap_msg_att* msg_att);

    //Function wrapper for updateUI
    std::function<void()> refreshUI;

    //getter function for imap session
    mailimap* get_imap();

    ~Session();
    
  };
  
}

#endif /* IMAP_H */
