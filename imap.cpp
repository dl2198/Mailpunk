#include "imap.hpp"
#include <cstring>

namespace IMAP {

  Session::Session(std::function<void()> updateUI)
  {
    refreshUI = updateUI;
    imap = mailimap_new(0, nullptr);
  }

  void Session::connect(std::string const& server, size_t port)
  {
    errorcode = mailimap_socket_connect(imap, server.c_str(), port);
    check_error(errorcode, "Could not connect to the server.");
  }

  void Session::login(std::string const& userid, std::string const& password)
  {
    errorcode = mailimap_login(imap, userid.c_str(), password.c_str());
    check_error(errorcode, "Could not login.");
    loggedin = true;
  }

  void Session::selectMailbox(std::string const& mailbox)
  {
    mb.assign(mailbox);

    if (loggedin)
      {
        errorcode = mailimap_select(imap, mailbox.c_str());
        check_error(errorcode, "Could not select mailbox.");
      }
  }

  void Session::fetch_message_no()
  {
    sa_list = mailimap_status_att_list_new_empty();
    
    errorcode = mailimap_status_att_list_add(sa_list,
					     MAILIMAP_STATUS_ATT_MESSAGES);
    check_error(errorcode, "Could not request number of messages.");
    
    errorcode = mailimap_status(imap, mb.c_str(), sa_list, &sa_result);
    check_error(errorcode, "Could not fetch number of messages.");
    
    message_no =
      ((struct mailimap_status_info*)clist_content(clist_begin(sa_result->st_info_list)))->st_value;
    free_status_memory();
  }

  void Session::free_status_memory()
  {
    if (sa_list)
      {
        mailimap_status_att_list_free(sa_list);
        sa_list = nullptr;
      }
    if (sa_result)
      {
        mailimap_mailbox_data_status_free(sa_result);
        sa_result = nullptr;
      }
  }

  void Session::free_messages()
  {
    if (messages)
      {
        for (int i = 0; messages[i]; i++)
          if (messages[i])
            delete messages[i];
        delete[] messages;
        messages = nullptr;
      }            
  }
  
  Message** Session::getMessages()
  {
    free_messages();
    fetch_message_no();

    if (message_no == 0)
      {
        messages = new Message*[1];
        messages[0] = nullptr;
        return messages;
      }

    if (message_no > 0)
      {
        messages = new Message*[message_no+1];

        set = mailimap_set_new_interval(1, 0);
        fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();

        fetch_att = mailimap_fetch_att_new_uid();
        errorcode = mailimap_fetch_type_new_fetch_att_list_add(fetch_type,
							       fetch_att);
        check_error(errorcode, "Could not request message IDs.");

        errorcode = mailimap_fetch(imap, set, fetch_type, &fetch_result);
        check_error(errorcode, "Could not fetch message IDs.");

        int i = 0;
        clistiter* cur;
        for (cur = clist_begin(fetch_result); cur != nullptr;
	     cur = clist_next(cur))
          {
            auto msg_att = (struct mailimap_msg_att*)clist_content(cur);
            auto uid = fetch_uid(msg_att);
            if (uid == 0)
              continue;
            messages[i] = new Message();
            messages[i]->set_uid(uid);
            messages[i]->set_imap(this);
            i++;
          }
        messages[i] = nullptr;

        free_fetch_memory();
      }
    
    return messages;
  }

  void Session::free_fetch_memory()
  {
    if (fetch_result)
      {
        mailimap_fetch_list_free(fetch_result);
        fetch_result = nullptr;
      }
    if (fetch_type)
      {
        mailimap_fetch_type_free(fetch_type);
        fetch_type = nullptr;
      }
    if (set)
      {
        mailimap_set_free(set);
        set = nullptr;
      }
  }
  
  uint32_t Session::fetch_uid(mailimap_msg_att* msg_att)
  {
    clistiter* cur;
    for (cur = clist_begin(msg_att->att_list); cur != nullptr;
	 cur = clist_next(cur))
      {
        auto item = (struct mailimap_msg_att_item*)clist_content(cur);
        if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC)
          continue;
        if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_UID)
          continue;
        return item->att_data.att_static->att_data.att_uid;
      }
    return 0;
  }

  mailimap* Session::get_imap()
  {
    return imap;
  }
 
  Session::~Session()
  {
    free_messages();

    if (loggedin)
      {
        errorcode = mailimap_logout(imap);
        if (errorcode != 4)
          check_error(errorcode, "Could not logout.");
      }

    free_fetch_memory();
    free_status_memory();
    
    mailimap_free(imap);
  }

  void Message::set_uid(uint32_t uid)
  {
    this->uid = uid;
  }

  void Message::set_imap(Session* imap)
  {
    this->session = imap;
  }

  std::string Message::getBody()
  {
    single_message = mailimap_set_new_single(uid);
    fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    section = mailimap_section_new(nullptr);
    
    fetch_att = mailimap_fetch_att_new_body_section(section);
    errorcode = mailimap_fetch_type_new_fetch_att_list_add(fetch_type,
							   fetch_att);
    check_error(errorcode, "Could not request body.");
    
    errorcode = mailimap_uid_fetch(session->get_imap(), single_message,
				   fetch_type, &fetch_result);
    check_error(errorcode, "Could not fetch body.");

    clistiter* cur;
    for (cur = clist_begin(fetch_result); cur != nullptr; cur = clist_next(cur))
      {
        auto msg_att = (struct mailimap_msg_att*)clist_content(cur);
        body = fetch_msg_data(msg_att);
      }

    free_fetch_memory();
    
    return body;
  }

  void Message::free_fetch_memory()
  {
    if (fetch_result)
      {
        mailimap_fetch_list_free(fetch_result);
        fetch_result = nullptr;
      }
    if (fetch_type)
      {
        mailimap_fetch_type_free(fetch_type);
        fetch_type = nullptr;
      }
    if (single_message)
      {
        mailimap_set_free(single_message);
        single_message = nullptr;
      }
  }

  std::string Message::fetch_msg_data(struct mailimap_msg_att* msg_att)
  {
    std::string msg_content;
    char* msg_content_cstring;
    clistiter * cur;
    
    for (cur = clist_begin(msg_att->att_list); cur != nullptr;
	 cur = clist_next(cur))
      {
        auto item = (struct mailimap_msg_att_item*)clist_content(cur);
        if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC)
          continue;
        if (item->att_data.att_static->att_type !=
	    MAILIMAP_MSG_ATT_BODY_SECTION)
          continue;
        msg_content_cstring =
	  item->att_data.att_static->att_data.att_body_section->sec_body_part;
        if (msg_content_cstring == nullptr)
          return "";
        msg_content = msg_content_cstring;
      }
    
    return msg_content;
  }

  std::string Message::getField(std::string fieldname)
  {
    char* field_cstring = (char*)malloc(fieldname.length() + 1);
    strcpy(field_cstring, fieldname.c_str());
    clist* hdr_list = clist_new();
    clist_append(hdr_list, field_cstring);

    header_list = mailimap_header_list_new(hdr_list);
    
    single_message = mailimap_set_new_single(uid);
    fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    section = mailimap_section_new_header_fields(header_list);
    
    fetch_att = mailimap_fetch_att_new_body_section(section);
    errorcode = mailimap_fetch_type_new_fetch_att_list_add(fetch_type,
							   fetch_att);
    check_error(errorcode, "Could not request header fields.");
    
    errorcode = mailimap_uid_fetch(session->get_imap(), single_message,
				   fetch_type, &fetch_result);
    check_error(errorcode, "Could not fetch header fields.");

    clistiter* cur;
    for (cur = clist_begin(fetch_result); cur != nullptr; cur = clist_next(cur))
      {
        auto msg_att = (struct mailimap_msg_att*)clist_content(cur);
        field = fetch_msg_data(msg_att);
        field = field.erase(0, field.find(' '));
        field = field.erase(0, 1);
        field = field.erase(field.length() - 4, 4);
      }

    free_fetch_memory();

    return field;

  }

  void Message::deleteFromMailbox()
  {
    single_message = mailimap_set_new_single(uid);
    flags = mailimap_flag_list_new_empty();
    
    deleted = mailimap_flag_new_deleted();
    errorcode = mailimap_flag_list_add(flags, deleted);
    check_error(errorcode, "Could not create delete flag.");
    
    store_att = mailimap_store_att_flags_new_set_flags(flags);
    errorcode = mailimap_uid_store(session->get_imap(),
				   single_message, store_att);
    check_error(errorcode, "Could not flag email for deletion.");

    errorcode = mailimap_expunge(session->get_imap());
    check_error(errorcode, "Could not delete email.");

    free_delete_memory();

    session->refreshUI();
  }

  void Message::free_delete_memory()
  {
    if (store_att)
      {
        mailimap_store_att_flags_free(store_att);
        store_att = nullptr;
      }
    if (single_message)
      {
        mailimap_set_free(single_message);
        single_message = nullptr;
      }
  }
  
}
