
#include "../include/smtp_server.h"

using namespace std;

int main(int argc, char *argv[])
{
    // run SMTP server to receive external emails
    SMTPServer::run(8090);

	return 0;
}