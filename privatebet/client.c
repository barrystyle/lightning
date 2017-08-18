/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

bits256 Clientrhash,Hostrhashes[CARDS777_MAXPLAYERS+1];
char Host_channel[64];

bits256 BET_clientrhash()
{
    return(Clientrhash);
}

void BET_sendpay(bits256 rhash,uint64_t satoshis)
{
    cJSON *routejson,*retjson,*array = cJSON_CreateArray();
    routejson = cJSON_CreateObject();
    jaddstr(routejson,"id",Host_peerid);
    jaddstr(routejson,"channel",Host_channel);
    jaddnum(routejson,"msatoshi",satoshis*1000);
    jaddnum(routejson,"delay",10);
    jaddi(array,routejson);
    // route { "id" : "02779b57b66706778aa1c7308a817dc080295f3c2a6af349bb1114b8be328c28dc", "channel" : "27446:1:0", "msatoshi" : 1000000, "delay" : 10 }
    // replace rhash in route
    if ( (retjson= chipsln_sendpay(array,rhash)) != 0 )
    {
        printf("sendpay %.8f to %s -> %s\n",dstr(satoshis),jprint(array,0),jprint(retjson,0));
        free_json(retjson);
    }
    free_json(array);
}

int32_t BET_client_join(cJSON *argjson,struct privatebet_info *bet,struct privatebet_vars *vars,int32_t senderid)
{
    cJSON *array,*pubkeys,*retjson,*channels,*item; int32_t i,n,already_connected,len; bits256 hash; char *idstr,*source,*dest,*short_id;
    if ( Host_peerid[0] == 0 )
    {
        safecopy(Host_peerid,jstr(argjson,"hostid"),sizeof(Host_peerid));
        already_connected = 0;
        if ( (retjson= chipsln_getpeers()) != 0 )
        {
            //{ "peers" :[{ "unique_id" : 0, "state" : "GOSSIPD", "netaddr" : "5.9.253.195:9735", "peerid" : "02779b57b66706778aa1c7308a817dc080295f3c2a6af349bb1114b8be328c28dc", "owner" : "lightningd_gossip" } ] }
            if ( (array= jarray(&n,retjson,"peers")) != 0 )
            {
                for (i=0; i<n; i++)
                {
                    item = jitem(array,i);
                    printf("(%s)\n",jprint(item,0));
                    if ( (idstr= jstr(item,"peerid")) != 0 && strcmp(idstr,Host_peerid) == 0 )
                    {
                        already_connected = 1;
                        break;
                    }
                }
            }
            free_json(retjson);
        }
        if ( already_connected == 0 && (retjson= chipsln_connect(Host_ipaddr,LN_port,Host_peerid)) != 0 )
        {
            printf("(%s:%u %s) CONNECTLN.(%s)\n",Host_ipaddr,LN_port,Host_peerid,jprint(retjson,0));
            if ( (idstr= jstr(retjson,"id")) != 0 && strcmp(idstr,Host_peerid) == 0 )
                already_connected = 1;
            free_json(retjson);
        }
        if ( already_connected != 0 && Host_channel[0] == 0 )
        {
            printf("getchannels\n");
            if ( (channels= chipsln_getchannels()) != 0 )
            {
                printf("got.(%s)\n",jprint(channels,0));
                if ( (array= jarray(&n,channels,"channels")) != 0 )
                {
                    for (i=0; i<n; i++)
                    {
                        item = jitem(array,i);
                        source = jstr(item,"source");
                        dest = jstr(item,"destination");
                        short_id = jstr(item,"short_id");
                        printf("source.%s dest.%s myid.%s Host.%s short.%s\n",source,dest,LN_idstr,Host_peerid,short_id);
                        if ( source != 0 && dest != 0 && strcmp(source,LN_idstr) == 0 && strcmp(dest,Host_peerid) == 0 && short_id != 0 )
                        {
                            len = strlen(short_id);
                            if ( len > 3 && short_id[len-2] == '/' )
                            {
                                strcpy(Host_channel,short_id);
                                Host_channel[len-2] = 0;
                                printf("Host_channel.(%s)\n",Host_channel);
                            }
                        }
                    }
                }
                free_json(channels);
            }
//{ "channels" :[{ "source" : "02779b57b66706778aa1c7308a817dc080295f3c2a6af349bb1114b8be328c28dc", "destination" : "03b03efcf647e6dd48b949e5b1f7e9e064257a5c48c2d56b1334b283b48338f821", "active" : true, "fee_per_kw" : 10, "last_update" : 1502988553, "flags" : 0, "delay" : 10, "short_id" : "27446:1:0/0" }, { "source" : "03b03efcf647e6dd48b949e5b1f7e9e064257a5c48c2d56b1334b283b48338f821", "destination" : "02779b57b66706778aa1c7308a817dc080295f3c2a6af349bb1114b8be328c28dc", "active" : true, "fee_per_kw" : 10, "last_update" : 1502988553, "flags" : 1, "delay" : 10, "short_id" : "27446:1:0/1" } ] }
                
        }
        // else open channel
    }
    if ( (array= jarray(&n,argjson,"hostrhash")) != 0 )
    {
        for (i=0; i<n; i++)
            Hostrhashes[i] = jbits256i(array,i);
        if ( (array= jarray(&n,argjson,"pubkeys")) != 0 && bet->chipsize == jint(argjson,"chipsize") )
        {
            for (i=0; i<n; i++)
            {
                hash = jbits256i(array,i);
                if ( bits256_cmp(hash,Mypubkey) == 0 && Host_peerid[0] != 0 && Host_channel[0] != 0 )
                {
                    printf("BET_sendpay\n");
                    BET_sendpay(Hostrhashes[i],bet->chipsize);
                    break;
                }
            }
        }
    }
    printf("JOIN broadcast.(%s)\n",jprint(argjson,0));
    return(0);
}

void BET_client_turnisend(struct privatebet_info *bet,struct privatebet_vars *vars)
{
    cJSON *cmdjson;
    if ( bet->myplayerid < bet->numplayers && bits256_cmp(bet->playerpubs[vars->turni],Mypubkey) == 0 )
    {
        cmdjson = cJSON_CreateObject();
        jaddstr(cmdjson,"method","turni");
        jaddbits256(cmdjson,"tableid",bet->tableid);
        jaddnum(cmdjson,"round",vars->round);
        jaddnum(cmdjson,"turni",vars->turni);
        jaddbits256(cmdjson,"pubkey",Mypubkey);
        jadd(cmdjson,"actions",BET_statemachine_turni_actions(bet,vars));
        printf("send TURNI.(%s)\n",jprint(cmdjson,0));
        BET_message_send("BET_client_turnisend",bet->pushsock,cmdjson,1,bet);
    }
}

void BET_client_turninext(struct privatebet_info *bet,struct privatebet_vars *vars)
{
    cJSON *reqjson;
    printf("BET_turni_next (%d, %d) numplayers.%d range.%d\n",vars->turni,vars->round,bet->numplayers,bet->range);
    if ( IAMHOST == 0 && vars->validperms == 0 )
        return;
    if ( bits256_cmp(bet->tableid,Mypubkey) != 0 )
        Lastturni = (uint32_t)time(NULL);
    vars->turni++;
    if ( vars->turni >= bet->numplayers )
    {
        printf("Round.%d completed\n",vars->round);
        BET_statemachine_endround(bet,vars);
        reqjson = cJSON_CreateObject();
        jaddstr(reqjson,"method","roundend");
        jaddnum(reqjson,"round",vars->round);
        jaddbits256(reqjson,"pubkey",Mypubkey);
        BET_message_send("BET_round",bet->pubsock>=0?bet->pubsock:bet->pushsock,reqjson,1,bet);
        vars->round++;
        vars->turni = 0;
        if ( vars->round >= bet->numrounds )
        {
            BET_tablestatus_send(bet,vars);
            Gamestarted = 0;
            vars->round = 0;
            Gamestart = (uint32_t)time(NULL) + BET_GAMESTART_DELAY;
            printf("Game completed next start.%u vs %u\n------------------\n\n",Gamestart,(uint32_t)time(NULL));
        }
    }
    /*if ( vars->turni == bet->myplayerid && vars->round < bet->numrounds && vars->roundready == vars->round )
    {
        BET_client_turnisend(bet,vars);
    }*/
}

int32_t BET_client_turni(cJSON *argjson,struct privatebet_info *bet,struct privatebet_vars *vars,int32_t senderid)
{
    struct privatebet_vars argvars; int32_t n;
    //printf("client TURNI.(%s) senderid.%d valid.%d\n",jprint(argjson,0),senderid,vars->validperms);
    if ( (IAMHOST != 0 || vars->validperms != 0) && senderid >= 0 && senderid < bet->numplayers )
    {
        memset(&argvars,0,sizeof(argvars));
        BET_betvars_parse(bet,&argvars,argjson);
        if ( vars->round < bet->numrounds )
        {
            if ( vars->actions[vars->round][senderid] != 0 )
                free_json(vars->actions[vars->round][senderid]);
            vars->actions[vars->round][senderid] = jduplicate(jarray(&n,argjson,"actions"));
            printf("round.%d senderid.%d (%s)\n",vars->round,senderid,jprint(vars->actions[vars->round][senderid],0));
            if ( argvars.turni == vars->turni && argvars.round == vars->round )
            {
                BET_client_turninext(bet,vars);
                //printf("new Turni.%d Round.%d\n",Turni,Round);
            }
        }
    }
    return(0);
}

int32_t BET_client_tablestatus(cJSON *msgjson,struct privatebet_info *bet,struct privatebet_vars *vars)
{
    BET_betinfo_parse(bet,vars,msgjson);
    /*if ( vars->turni == bet->myplayerid )
        BET_client_turnisend(bet,vars);*/
    return(0);
}

int32_t BET_client_gamestart(cJSON *argjson,struct privatebet_info *bet,struct privatebet_vars *vars)
{
    cJSON *cmdjson;
    cmdjson = cJSON_CreateObject();
    jaddstr(cmdjson,"method","started");
    jaddstr(cmdjson,"game",bet->game);
    jaddbits256(cmdjson,"tableid",bet->tableid);
    jaddnum(cmdjson,"numrounds",bet->numrounds);
    jaddnum(cmdjson,"range",bet->range);
    jaddbits256(cmdjson,"pubkey",Mypubkey);
    jadd(cmdjson,"actions",BET_statemachine_gamestart_actions(bet,vars));
    if ( bits256_nonz(vars->myhash) == 0 )
    {
        BET_permutation(vars->mypermi,bet->range);
        vcalc_sha256(0,vars->myhash.bytes,(uint8_t *)vars->mypermi,sizeof(*vars->mypermi) * bet->range);
    }
    jaddbits256(cmdjson,"hash",vars->myhash);
    BET_message_send("BET_gamestarted",bet->pushsock,cmdjson,1,bet);
    BET_statemachine_roundstart(bet,vars);
    return(0);
}

int32_t BET_client_gamestarted(cJSON *argjson,struct privatebet_info *bet,struct privatebet_vars *vars,int32_t senderid)
{
    int32_t i; cJSON *array,*reqjson; char str[65];
    if ( senderid >= 0 && senderid <= bet->numplayers )
    {
        vars->hashes[senderid][0] = jbits256(argjson,"hash");
        for (i=0; i<bet->numplayers; i++)
            if ( bits256_nonz(vars->hashes[i][0]) == 0 )
                break;
        if ( i == bet->numplayers )
        {
            array = cJSON_CreateArray();
            for (i=0; i<bet->range; i++)
                jaddinum(array,vars->mypermi[i]);
            reqjson = cJSON_CreateObject();
            jaddstr(reqjson,"method","perm");
            jadd(reqjson,"perm",array);
            jaddbits256(reqjson,"pubkey",Mypubkey);
            BET_message_send("BET_perm",bet->pubsock>=0?bet->pubsock:bet->pushsock,reqjson,1,bet);
        } //else printf("i.%d != num.%d senderid.%d process gamestarted.(%s) [sender.%d] <- %s\n",i,bet->numplayers,senderid,jprint(argjson,0),senderid,bits256_str(str,vars->hashes[senderid][0]));
    }
    return(0);
}

int32_t BET_client_perm(cJSON *argjson,struct privatebet_info *bet,struct privatebet_vars *vars,int32_t senderid)
{
    int32_t i,n,j; cJSON *array;
    //printf("got perm.(%s) sender.%d\n",jprint(argjson,0),senderid);
    if ( senderid >= 0 && senderid < bet->numplayers )
    {
        if ( (array= jarray(&n,argjson,"perm")) != 0 && n == bet->range )
        {
            for (i=0; i<bet->range; i++)
                vars->permis[senderid][i] = jinti(array,i);
            vcalc_sha256(0,vars->hashes[senderid][1].bytes,(uint8_t *)vars->permis[senderid],sizeof(*vars->permis[senderid]) * bet->range);
        }
        for (i=0; i<bet->numplayers; i++)
        {
            if ( bits256_cmp(vars->hashes[i][0],vars->hashes[i][1]) != 0 )
            {
                //char str[65],str2[65]; printf("%d: %s != %s\n",i,bits256_str(str,vars->hashes[i][0]),bits256_str(str2,vars->hashes[i][1]));
                break;
            }
        }
        if ( i == bet->numplayers )
        {
            vars->validperms = 1;
            vars->roundready = 0;
            vars->turni = 0;
            j = BET_permutation_sort(vars->permi,vars->permis,bet->numplayers,bet->range);
            for (i=0; i<bet->range; i++)
                printf("%d ",vars->permi[i]);
            printf("validated perms best.%d\n",j);
            /*if ( vars->turni == bet->myplayerid && vars->round == 0 )
                BET_client_turnisend(bet,vars);*/
        }
    }
    return(0);
}

int32_t BET_client_endround(cJSON *argjson,struct privatebet_info *bet,struct privatebet_vars *vars,int32_t senderid)
{
    int32_t i;
    if ( senderid >= 0 && senderid < bet->numplayers )
    {
        if ( vars->endround[senderid] == 0 )
            vars->endround[senderid] = (uint32_t)time(NULL);
        for (i=0; i<bet->numplayers; i++)
            if ( vars->endround[i] == 0 || vars->actions[vars->roundready][i] == 0 )
                break;
        if ( i == bet->numplayers )
        {
            vars->roundready++;
            BET_statemachine_roundstart(bet,vars);
            /*if ( vars->turni == bet->myplayerid && vars->round == vars->roundready && vars->round < bet->numrounds )
                BET_client_turnisend(bet,vars);*/
        }
    }
    return(0);
}

int32_t BET_client_MofN(cJSON *argjson,struct privatebet_info *bet,struct privatebet_vars *vars,int32_t senderid)
{
    int32_t cardi,playerj,recvlen; bits256 shard,*ptr; char *cipherstr; uint8_t decoded[sizeof(bits256) + 1024],encoded[sizeof(bits256) + 1024];
    cardi = jint(argjson,"cardi");
    playerj = jint(argjson,"playerj");
    memset(shard.bytes,0,sizeof(shard));
    if ( cardi >= 0 && cardi < bet->range && playerj >= 0 && playerj < bet->numplayers && senderid >= 0 && senderid < bet->numplayers && playerj == bet->myplayerid )
    {
        if ( (cipherstr= jstr(argjson,"cipher")) != 0 )
        {
            char str[65];
            recvlen = (int32_t)strlen(cipherstr) >> 1;
            decode_hex(encoded,recvlen,cipherstr);
            if ( (ptr= (bits256 *)BET_decrypt(decoded,sizeof(decoded),bet->playerpubs[senderid],Myprivkey,encoded,&recvlen)) == 0 )
                printf("decrypt error cardi.%d playerj.%d %s\n",cardi,playerj,bits256_str(str,Myprivkey));
            else shard = *ptr;
            //printf("cipherstr.(%s) -> %s\n",cipherstr,bits256_str(str,shard));
        } else shard = jbits256(argjson,"shard");
        //char str[65],str2[65]; printf("client MofN: cardi.%d playerj.%d %s shardi.%d bet->deckid.%s\n",cardi,playerj,bits256_str(str,shard),senderid,bits256_str(str2,bet->deckid));
        BET_MofN_item(bet->deckid,cardi,bet->cardpubs,bet->range,playerj,bet->numplayers,shard,senderid);
    }
    return(0);
}

int32_t BET_senderid(cJSON *argjson,struct privatebet_info *bet)
{
    int32_t i; char *peerid = jstr(argjson,"peerid");
    for (i=0; i<bet->numplayers; i++)
        if ( bits256_cmp(jbits256(argjson,"pubkey"),bet->playerpubs[i]) == 0 )
            return(i);
    if ( peerid != 0 && strcmp(Host_peerid,peerid) == 0 )
        return(bet->maxplayers);
    return(-1);
}

int32_t BET_clientupdate(cJSON *argjson,uint8_t *ptr,int32_t recvlen,struct privatebet_info *bet,struct privatebet_vars *vars) // update game state based on host broadcast
{
    static uint8_t *decoded; static int32_t decodedlen;
    char *method; int32_t senderid; bits256 *MofN;
    if ( (method= jstr(argjson,"method")) != 0 )
    {
        senderid = BET_senderid(argjson,bet);
        //printf("BET_clientupdate: pushsock.%d subsock.%d method.%s sender.%d\n",bet->pushsock,bet->subsock,method,senderid);
        if ( strcmp(method,"tablestatus") == 0 )
            return(BET_client_tablestatus(argjson,bet,vars));
        else if ( strcmp(method,"turni") == 0 )
            return(BET_client_turni(argjson,bet,vars,senderid));
        else if ( strcmp(method,"roundend") == 0 )
            return(BET_client_endround(argjson,bet,vars,senderid));
        else if ( strcmp(method,"start0") == 0 )
        {
            bet->numrounds = jint(argjson,"numrounds");
            bet->numplayers = jint(argjson,"numplayers");
            bet->range = jint(argjson,"range");
            return(0);
        }
        else if ( strcmp(method,"start") == 0 )
            return(BET_client_gamestart(argjson,bet,vars));
        else if ( strcmp(method,"started") == 0 )
            return(BET_client_gamestarted(argjson,bet,vars,senderid));
        else if ( strcmp(method,"perm") == 0 )
            return(BET_client_perm(argjson,bet,vars,senderid));
        else if ( strcmp(method,"deali") == 0 )
            return(BET_client_deali(argjson,bet,vars,senderid));
        else if ( strcmp(method,"join") == 0 )
            return(BET_client_join(argjson,bet,vars,senderid));
        else if ( strcmp(method,"MofN") == 0 )
            return(BET_client_MofN(argjson,bet,vars,senderid));
        else if ( strcmp(method,"deckpacket") == 0 )
        {
            if ( decodedlen < recvlen )
            {
                decoded = realloc(decoded,recvlen);
                decodedlen = recvlen;
                printf("alloc decoded[%d]\n",recvlen);
            }
            if ( (MofN= BET_process_packet(bet->cardpubs,&bet->deckid,GENESIS_PUBKEY,Myprivkey,decoded,decodedlen,Mypubkey,ptr,recvlen,bet->numplayers,bet->range)) == 0 )
            {
                //printf("error processing packet, most likely not encrypted to us\n");
                return(0);
            }
            memcpy(bet->MofN,MofN,sizeof(*MofN) * bet->numplayers * bet->range);
            //int32_t i; char str[65]; for (i=0; i<bet->numplayers * bet->range; i++)
            //    printf("%s ",bits256_str(str,bet->MofN[i]));
            //printf("MofN.%s\n",bits256_str(str,bet->deckid));
            return(0);
        }
    } else printf("clientupdate unexpected.(%s)\n",jprint(argjson,0));
    return(-1);
}

void BET_clientloop(void *_ptr)
{
    uint32_t lasttime = 0; int32_t lastround=-1,nonz,recvlen; uint16_t port=7798; char connectaddr[64],hostip[64]; void *ptr; cJSON *msgjson,*reqjson; struct privatebet_vars *VARS; struct privatebet_info *bet = _ptr;
    VARS = calloc(1,sizeof(*VARS));
    strcpy(hostip,"5.9.253.195"); // jl777: get from BET blockchain
    printf("client loop: pushsock.%d subsock.%d\n",bet->pushsock,bet->subsock);
    while ( 1 )
    {
        if ( bet->subsock >= 0 && bet->pushsock >= 0 )
        {
            nonz = 0;
            if ( (recvlen= nn_recv(bet->subsock,&ptr,NN_MSG,0)) > 0 )
            {
                nonz++;
                if ( (msgjson= cJSON_Parse(ptr)) != 0 )
                {
                    if ( BET_clientupdate(msgjson,ptr,recvlen,bet,VARS) < 0 )
                        printf("unknown clientupdate msg.(%s)\n",jprint(msgjson,0));
                    free_json(msgjson);
                }
                nn_freemsg(ptr);
            }
            if ( nonz == 0 )
            {
                if ( time(NULL) > lasttime+5 )
                {
                    printf("%s round.%d turni.%d myid.%d | valid.%d roundready.%d\n",bet->game,VARS->round,VARS->turni,bet->myplayerid,VARS->validperms,VARS->roundready);
                    lasttime = (uint32_t)time(NULL);
                }
                usleep(10000);
                if ( VARS->validperms != 0 && VARS->turni == bet->myplayerid && VARS->roundready == VARS->round && lastround != VARS->round )
                {
                    BET_client_turnisend(bet,VARS);
                    lastround = VARS->round;
                }
            }
        }
        else if ( hostip[0] != 0 && port > 0 )
        {
            BET_transportname(0,connectaddr,hostip,port);
            safecopy(Host_ipaddr,hostip,sizeof(Host_ipaddr));
            printf("connect %s\n",connectaddr);
            bet->subsock = BET_nanosock(0,connectaddr,NN_SUB);
            BET_transportname(0,connectaddr,hostip,port+1);
            bet->pushsock = BET_nanosock(0,connectaddr,NN_PUSH);
            reqjson = cJSON_CreateObject();
            jaddbits256(reqjson,"pubkey",Mypubkey);
            jaddstr(reqjson,"method","join");
            jaddstr(reqjson,"peerid",LN_idstr);
            Clientrhash = chipsln_rhash_create(bet->chipsize,"0");
            BET_message_send("BET_havetable",bet->pushsock,reqjson,1,bet);
        }
        else
        {
            // update list of tables
        }
    }
}


