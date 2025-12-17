extern void outbyte(unsigned char c);
extern char inbyte();

int read(int fd, char *buf, int nbytes)
{
  char c;
  int  i;
  int ch;
  if(fd==0 || fd==2 || fd==3)ch=0;
  else if(fd==4)ch=1;
  else return -1;//error
    for (i = 0; i < nbytes; i++) {
    c = inbyte(ch);

    if (c == '\r' || c == '\n'){ /* CR -> CRLF */
      outbyte('\r',ch);
      outbyte('\n',ch);
      *(buf + i) = '\n';

    /* } else if (c == '\x8'){ */     /* backspace \x8 */
    } else if (c == '\x7f'){      /* backspace \x8 -> \x7f (by terminal config.) */
      if (i > 0){
	outbyte('\x8',ch); /* bs  */
	outbyte(' ',ch);   /* spc */
	outbyte('\x8',ch); /* bs  */
	i--;
      }
      i--;
      continue;

    } else {
      outbyte(c,ch);
      *(buf + i) = c;
    }

    if (*(buf + i) == '\n'){
      return (i + 1);
    }
  }
  return (i);
}

int write (int fd, char *buf, int nbytes)
{
  int i, j;
  int ch;
  if(fd==0 || fd==3)ch=0;
  else if(fd==4)ch=1;
  else return -1;//error
  
  for (i = 0; i < nbytes; i++) {
    if (*(buf + i) == '\n') {
      outbyte ('\r',ch);          /* LF -> CRLF */
    }
    outbyte (*(buf + i),ch);
    for (j = 0; j < 300; j++);
  }
  return (nbytes);
}
