#ifndef _SMARTCONFIG_ACK_H
#define _SMARTCONFIG_ACK_H

typedef enum {
    SC_ACK_TYPE_ESPTOUCH = 0,                       /*!< ESP touch ACK type */
    SC_ACK_TYPE_AIRKISS,                            /*!< Airkiss ACK type */
} sc_ack_type_t;

/**
 * @brief Smartconfig parameters passed to sc_ack_send call.
 */
typedef struct sc_ack {
    sc_ack_type_t type;           /*!< Smartconfig ACK type */
    uint8_t *link_flag;           /*!< Smartconfig link flag */
    sc_callback_t cb;             /*!< Smartconfig callback function */
    struct {
        uint8_t token;            /*!< Smartconfig token to be sent */
        uint8_t mac[6];           /*!< MAC address of station */
        uint8_t ip[4];            /*!< IP address of cellphone */
    } ctx;
} sc_ack_t;


void sc_ack_send(sc_ack_t *param) { }
void sc_ack_send_stop(void) { }


#endif


