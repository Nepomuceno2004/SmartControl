#include "pico/stdlib.h"     // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "pico/unique_id.h"  // Biblioteca com recursos para trabalhar com os pinos GPIO do Raspberry Pi Pico

#include "hardware/gpio.h" // Biblioteca de hardware de GPIO
#include "hardware/irq.h"  // Biblioteca de hardware de interrupções
#include "hardware/adc.h"  // Biblioteca de hardware para conversão ADC

#include "lwip/apps/mqtt.h"      // Biblioteca LWIP MQTT -  fornece funções e recursos para conexão MQTT
#include "lwip/apps/mqtt_priv.h" // Biblioteca que fornece funções e recursos para Geração de Conexões
#include "lwip/dns.h"            // Biblioteca que fornece funções e recursos suporte DNS:
#include "lwip/altcp_tls.h"      // Biblioteca que fornece funções e recursos para conexões seguras usando TLS:
#include "lib/matrizLed.h"
#include "lib/joystick.h"

// -------------------------- definições dos periféricos ------------------------
// variáveis das condições
double temperatura = 25.0; // Começa com 25°C
double ph = 7.0;           // Começa 7.0 de ph

// limites do ph
#define MAX_PH 14.0
#define MIN_PH 0.0

#define STEP 0.01 // Passo de variação para cada movimento do joystick

// pino do botão
#define botaoB 6

// matrizes que definem quais leds acendem
bool escritorio[NUM_PIXELS] = {
    1, 1, 0, 0, 0,
    0, 0, 0, 1, 1,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0};

bool quarto[NUM_PIXELS] = {
    0, 0, 0, 1, 1,
    1, 1, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0};

bool garagem[NUM_PIXELS] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 1, 1,
    1, 1, 0, 0, 0};

bool sala[NUM_PIXELS] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    1, 1, 0, 0, 0,
    0, 0, 0, 1, 1};

// -------------------------- definições do mqtt ------------------------
#define WIFI_SSID "SEU_SSID"              // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASSWORD "SEU_PASSORD_WIFI"  // Substitua pela senha da sua rede Wi-Fi
#define MQTT_SERVER "SEU_HOST"            // Substitua pelo endereço do host - broket MQTT: Ex: 192.168.1.107
#define MQTT_USERNAME "SEU_USERNAME_MQTT" // Substitua pelo nome da host MQTT - Username
#define MQTT_PASSWORD "SEU_PASSWORD_MQTT" // Substitua pelo Password da host MQTT - credencial de acesso - caso exista

#ifndef MQTT_SERVER
#error Need to define MQTT_SERVER
#endif

// This file includes your client certificate for client server authentication
#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

// Dados do cliente MQTT
typedef struct
{
    mqtt_client_t *mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

#ifndef DEBUG_printf
#ifndef NDEBUG
#define DEBUG_printf printf
#else
#define DEBUG_printf(...)
#endif
#endif

#ifndef INFO_printf
#define INFO_printf printf
#endif

#ifndef ERROR_printf
#define ERROR_printf printf
#endif

// Temporização da coleta de temperatura
#define TEMP_WORKER_TIME_S 10

// Manter o programa ativo - keep alive in seconds
#define MQTT_KEEP_ALIVE_S 60

// QoS - mqtt_subscribe
// At most once (QoS 0)
// At least once (QoS 1)
// Exactly once (QoS 2)
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0

// Tópico usado para: last will and testament
#define MQTT_WILL_TOPIC "/online"
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1

#ifndef MQTT_DEVICE_NAME
#define MQTT_DEVICE_NAME "pico"
#endif

// Definir como 1 para adicionar o nome do cliente aos tópicos, para suportar vários dispositivos que utilizam o mesmo servidor
#ifndef MQTT_UNIQUE_TOPIC
#define MQTT_UNIQUE_TOPIC 0
#endif

// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err);

// Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name);

// Controle da matriz (simulação dos cômodos)
static void control_lights(MQTT_CLIENT_DATA_T *state, bool on, bool *comodo);

// Publicar temperatura
static void publish_condition(MQTT_CLIENT_DATA_T *state);

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err);

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err);

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T *state, bool sub);

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);

// Publicar temperatura
static void condition_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static async_at_time_worker_t condition_worker = {.do_work = condition_worker_fn};

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state);

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}
int main(void)
{

    // inicializa e configura o botão
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // Aqui termina o trecho para modo BOOTSEL com botão B

    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();
    INFO_printf("mqtt client starting\n");

    matriz_init();
    joystick_init();

    // Cria registro com os dados do cliente
    static MQTT_CLIENT_DATA_T state;

    // Inicializa a arquitetura do cyw43
    if (cyw43_arch_init())
    {
        panic("Failed to inizialize CYW43");
    }

    // Usa identificador único da placa
    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for (int i = 0; i < sizeof(unique_id_buf) - 1; i++)
    {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }

    // Gera nome único, Ex: pico1234
    char client_id_buf[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id_buf) - 1];
    memcpy(&client_id_buf[0], MQTT_DEVICE_NAME, sizeof(MQTT_DEVICE_NAME) - 1);
    memcpy(&client_id_buf[sizeof(MQTT_DEVICE_NAME) - 1], unique_id_buf, sizeof(unique_id_buf) - 1);
    client_id_buf[sizeof(client_id_buf) - 1] = 0;
    INFO_printf("Device name %s\n", client_id_buf);

    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S; // Keep alive in sec
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;
#else
    state.mqtt_client_info.client_user = NULL;
    state.mqtt_client_info.client_pass = NULL;
#endif
    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(&state, MQTT_WILL_TOPIC), sizeof(will_topic));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // TLS enabled
#ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    // This confirms the indentity of the server and the client
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
                                                                                client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
#if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
    WARN_printf("Warning: tls without verification is insecure\n");
#endif
#else
    state->client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
    WARN_printf("Warning: tls without a certificate is insecure\n");
#endif
#endif

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        panic("Failed to connect");
    }
    INFO_printf("\nConnected to Wifi\n");

    // Faz um pedido de DNS para o endereço IP do servidor MQTT
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    // Se tiver o endereço, inicia o cliente
    if (err == ERR_OK)
    {
        start_client(&state);
    }
    else if (err != ERR_INPROGRESS)
    { // ERR_INPROGRESS means expect a callback
        panic("dns request failed");
    }

    int16_t dx, dy;
    // Loop condicionado a conexão mqtt
    while (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst))
    {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(10000));

        joystick_leitura_corrigida(&dx, &dy);
        // printf("Temperatura X: %.2f, PH Y: %.2f\n", temperatura, ph);

        // Ajuste de temperatura com base no movimento no eixo X
        // Se o movimento for maior que 1000 altera a temperatura
        if (abs(dx) > 1000)
        {
            // Aumenta ou diminui a temperatura com base na direção do movimento
            temperatura += (dx > 0) ? STEP : -STEP;
        }

        // Ajuste de pH com base no movimento no eixo Y
        if (abs(dy) > 1000)
        {
            // Calcula o novo valor de pH antes de aplicar
            float novo_ph = ph + ((dy > 0) ? STEP : -STEP);

            // Garante que o pH fique dentro da faixa válida (0 a 14)
            if (novo_ph >= 0 && novo_ph <= 14)
            {
                ph = novo_ph;
            }
        }
    }

    INFO_printf("mqtt client exiting\n");
    return 0;
}

// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err)
{
    if (err != 0)
    {
        ERROR_printf("pub_request_cb failed %d", err);
    }
}

// Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name)
{
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

// Função que controla as luzes (LEDs) de um cômodo e publica o estado no MQTT.
// Parâmetros:
// - state: estrutura com dados do cliente MQTT
// - on: indica se a luz deve ser ligada (true) ou desligada (false)
// - comodo: ponteiro que identifica qual cômodo está sendo controlado
static void control_lights(MQTT_CLIENT_DATA_T *state, bool on, bool *comodo)
{
    // Define a mensagem a ser publicada com base no estado (ligado/desligado)
    const char *message = on ? "On" : "Off";

    // Se a luz deve ser LIGADA
    if (on)
    {
        if (comodo == garagem)
        {
            atualizarLeds(garagem, true); // Liga os LEDs da garagem
            mqtt_publish(state->mqtt_client_inst, full_topic(state, "/garagem/state"), message, strlen(message),
                         MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        }
        else if (comodo == sala)
        {
            atualizarLeds(sala, true); // Liga os LEDs da sala
            mqtt_publish(state->mqtt_client_inst, full_topic(state, "/sala/state"), message, strlen(message),
                         MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        }
        else if (comodo == quarto)
        {
            atualizarLeds(quarto, true); // Liga os LEDs do quarto
            mqtt_publish(state->mqtt_client_inst, full_topic(state, "/quarto/state"), message, strlen(message),
                         MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        }
        else if (comodo == escritorio)
        {
            atualizarLeds(escritorio, true); // Liga os LEDs do escritório
            mqtt_publish(state->mqtt_client_inst, full_topic(state, "/escritorio/state"), message, strlen(message),
                         MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        }
    }
    // Caso contrário, a luz deve ser DESLIGADA
    else
    {
        if (comodo == garagem)
        {
            atualizarLeds(garagem, false); // Desliga os LEDs da garagem
            mqtt_publish(state->mqtt_client_inst, full_topic(state, "/garagem/state"), message, strlen(message),
                         MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        }
        else if (comodo == sala)
        {
            atualizarLeds(sala, false); // Desliga os LEDs da sala
            mqtt_publish(state->mqtt_client_inst, full_topic(state, "/sala/state"), message, strlen(message),
                         MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        }
        else if (comodo == quarto)
        {
            atualizarLeds(quarto, false); // Desliga os LEDs do quarto
            mqtt_publish(state->mqtt_client_inst, full_topic(state, "/quarto/state"), message, strlen(message),
                         MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        }
        else if (comodo == escritorio)
        {
            atualizarLeds(escritorio, false); // Desliga os LEDs do escritório
            mqtt_publish(state->mqtt_client_inst, full_topic(state, "/escritorio/state"), message, strlen(message),
                         MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        }
    }

    return; // Retorno explícito, embora desnecessário para função void
}

// Função responsável por publicar os dados de temperatura e pH no MQTT,
// apenas se houver mudança nos valores em relação à última publicação.
static void publish_condition(MQTT_CLIENT_DATA_T *state)
{
    static float old_temperatura;
    const char *temperature_key = full_topic(state, "/aquario/temperature");

    // Verifica se a temperatura atual mudou em relação à última publicada
    if (temperatura != old_temperatura)
    {
        // Atualiza o valor da última temperatura publicada
        old_temperatura = temperatura;

        // Converte o valor da temperatura para string com uma casa decimal
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.1f", temperatura);

        // Imprime no log a mensagem que será publicada
        INFO_printf("Publishing %s to %s\n", temp_str, temperature_key);

        // Publica a temperatura no broker MQTT
        mqtt_publish(
            state->mqtt_client_inst, // Instância do cliente MQTT
            temperature_key,         // Tópico onde será publicada a temperatura
            temp_str,                // Payload (valor da temperatura como string)
            strlen(temp_str),        // Tamanho do payload
            MQTT_PUBLISH_QOS,        // Qualidade de serviço (QoS) da publicação
            MQTT_PUBLISH_RETAIN,     // Flag de retenção da mensagem
            pub_request_cb,          // Callback de confirmação de publicação
            state                    // Contexto do cliente
        );
    }

    static float old_ph;
    const char *ph_key = full_topic(state, "/aquario/ph");
    // Verifica se o valor de pH atual mudou em relação ao último publicado
    if (ph != old_ph)
    {
        // Atualiza o valor do último pH publicado
        old_ph = ph;

        // Converte o valor do pH para string com uma casa decimal
        char ph_str[16];
        snprintf(ph_str, sizeof(ph_str), "%.1f", ph);

        // Imprime no log a mensagem que será publicada
        INFO_printf("Publishing %s to %s\n", ph_str, ph_key);

        // Publica o valor de pH no broker MQTT
        mqtt_publish(
            state->mqtt_client_inst, // Instância do cliente MQTT
            ph_key,                  // Tópico onde será publicado o pH
            ph_str,                  // Payload (valor do pH como string)
            strlen(ph_str),          // Tamanho do payload
            MQTT_PUBLISH_QOS,        // Qualidade de serviço (QoS) da publicação
            MQTT_PUBLISH_RETAIN,     // Flag de retenção da mensagem
            pub_request_cb,          // Callback de confirmação de publicação
            state                    // Contexto do cliente
        );
    }
}

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (err != 0)
    {
        panic("subscribe request failed %d", err);
    }
    state->subscribe_count++;
}

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (err != 0)
    {
        panic("unsubscribe request failed %d", err);
    }
    state->subscribe_count--;
    assert(state->subscribe_count >= 0);

    // Stop if requested
    if (state->subscribe_count <= 0 && state->stop_client)
    {
        mqtt_disconnect(state->mqtt_client_inst);
    }
}

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T *state, bool sub)
{
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/garagem"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/sala"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/quarto"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/escritorio"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/print"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ping"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/exit"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
}

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
#if MQTT_UNIQUE_TOPIC
    const char *basic_topic = state->topic + strlen(state->mqtt_client_info.client_id) + 1;
#else
    const char *basic_topic = state->topic;
#endif
    strncpy(state->data, (const char *)data, len);
    state->len = len;
    state->data[len] = '\0';

    DEBUG_printf("Topic: %s, Message: %s\n", state->topic, state->data);
    if (strcmp(basic_topic, "/garagem") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            control_lights(state, true, garagem);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            control_lights(state, false, garagem);
    }
    else if (strcmp(basic_topic, "/sala") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            control_lights(state, true, sala);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            control_lights(state, false, sala);
    }
    else if (strcmp(basic_topic, "/quarto") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            control_lights(state, true, quarto);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            control_lights(state, false, quarto);
    }
    else if (strcmp(basic_topic, "/escritorio") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            control_lights(state, true, escritorio);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            control_lights(state, false, escritorio);
    }
    else if (strcmp(basic_topic, "/print") == 0)
    {
        INFO_printf("%.*s\n", len, data);
    }
    else if (strcmp(basic_topic, "/ping") == 0)
    {
        char buf[11];
        snprintf(buf, sizeof(buf), "%u", to_ms_since_boot(get_absolute_time()) / 1000);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/uptime"), buf, strlen(buf), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
    else if (strcmp(basic_topic, "/exit") == 0)
    {
        state->stop_client = true;      // stop the client when ALL subscriptions are stopped
        sub_unsub_topics(state, false); // unsubscribe
    }
}

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

// Função executada periodicamente para publicar as condições
static void condition_worker_fn(async_context_t *context, async_at_time_worker_t *worker)
{
    // Recupera os dados do cliente MQTT a partir do campo user_data do worker
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)worker->user_data;

    // Chama a função que verifica e publica as condições (se houver mudanças)
    publish_condition(state);

    // Agenda a próxima execução desta função após TEMP_WORKER_TIME_S segundos
    async_context_add_at_time_worker_in_ms(
        context,                  // Contexto assíncrono usado para agendamento
        worker,                   // Worker que será executado novamente
        TEMP_WORKER_TIME_S * 1000 // Tempo em milissegundos (conversão de segundos)
    );
}

// Callback chamado após tentativa de conexão MQTT (sucesso ou falha)
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    // Recupera a estrutura de dados do cliente MQTT passada como argumento
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;

    // Verifica se a conexão foi aceita com sucesso pelo broker
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        state->connect_done = true;
        sub_unsub_topics(state, true);

        if (state->mqtt_client_info.will_topic)
        {
            mqtt_publish(
                state->mqtt_client_inst,            // Cliente MQTT
                state->mqtt_client_info.will_topic, // Tópico de presença
                "1",                                // Payload indicando "online"
                1,                                  // Tamanho do payload
                MQTT_WILL_QOS,                      // QoS da publicação
                true,                               // Retenção da mensagem
                pub_request_cb,                     // Callback após publicação
                state                               // Contexto do cliente
            );
        }

        // Inicia o agendador assíncrono para publicar temperatura/pH periodicamente
        condition_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(
            cyw43_arch_async_context(), // Contexto assíncrono
            &condition_worker,          // Worker que será executado periodicamente
            0                           // Executar imediatamente a primeira vez
        );
    }
    // Caso o cliente tenha sido desconectado antes da conexão ser concluída
    else if (status == MQTT_CONNECT_DISCONNECTED)
    {
        if (!state->connect_done)
        {
            panic("Failed to connect to mqtt server"); // Aborta o programa com erro
        }
    }
    // Se o status recebido for inesperado, trata como erro crítico
    else
    {
        panic("Unexpected status"); // Aborta o programa com erro
    }
}

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state)
{
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Using TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Warning: Not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst)
    {
        panic("MQTT client instance creation error");
    }
    INFO_printf("IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    INFO_printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK)
    {
        panic("MQTT broker connection error");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

// Callback chamado quando a resolução de DNS é concluída (sucesso ou falha)
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;

    if (ipaddr)
    {
        // Armazena o endereço IP do servidor MQTT na estrutura de estado
        state->mqtt_server_address = *ipaddr;

        // Inicia o cliente MQTT com o endereço IP obtido
        start_client(state);
    }
    else
    {
        // Se a resolução de DNS falhou, gera um erro crítico e aborta
        panic("dns request failed");
    }
}
