extern void outbyte(unsigned char c, int fd);
extern char inbyte(int fd);
extern void outbyte(unsigned char c, int ch); // 引数chに合わせる
extern char inbyte(int ch);                   // 引数chに合わせる

int read(int fd, char *buf, int nbytes)
{
  char c;
  int  i;
  int ch;
  
  if(fd==0 || fd==2 || fd==3) ch=0;
  else if(fd==4) ch=1;
  else return -1; // error

  for (i = 0; i < nbytes; i++) {
    c = inbyte(ch);

    if (c == '\r' || c == '\n'){ /* CR -> CRLF */
      outbyte('\r', ch);
      outbyte('\n', ch);
      *(buf + i) = '\n';
    } 
    /* 以下の行で、コメント記号 /* が正しく閉じられていなかったり、
       バックスラッシュが不正に解釈されていたりしたことがエラーの原因です。
    */
    else if (c == '\x7f'){ /* backspace \x8 -> \x7f (by terminal config.) */
      if (i > 0){
        outbyte('\x8', ch); /* bs  */
        outbyte(' ', ch);   /* spc */
        outbyte('\x8', ch); /* bs  */
        i--;
      }
      i--;
      continue;
    } 
    else {
      outbyte(c, ch);
      *(buf + i) = c;
    }

    if (*(buf + i) == '\n'){
      return (i + 1);
    }
  }
  return (i);
}

int write(int fd, char *buf, int nbytes)
{
  int i, j;
  int ch;
  
  // ファイル記述子(fd)の割り当てを整理
  if(fd==0 || fd==1 || fd==2 || fd==3) ch=0;
  else if(fd==4) ch=1;
  else return -1; // error
  
  for (i = 0; i < nbytes; i++) {
    if (*(buf + i) == '\n') {
      outbyte ('\r', ch); /* LF -> CRLF */
    }
    outbyte (*(buf + i), ch);
    for (j = 0; j < 300; j++); 
  }
  return (nbytes);
}



void my_write(int fd, char *s){
 //文字の長さ指定不要のwrite
    int len = 0;
    while (s[len])len++; 
    write(fd, s, len);
}
