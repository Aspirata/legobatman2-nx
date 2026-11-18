int main(){
  unsigned checks=0;
  auto check=[&](bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}};
  auto flushable=[](const nouveau_horizon_channel& source){
    auto c=source;
    if(c.device->total_order_channels){
      const nouveau_horizon_fence fence{1,1};
      if(nouveau_horizon_channel_append_waits_locked(&c,1,&fence,true)!=NOUVEAU_HORIZON_SUCCESS)return false;
    }
    return nouveau_horizon_channel_reserve_entries_locked(&c,c.mapped_completion_enabled?2:1,0)==NOUVEAU_HORIZON_SUCCESS;
  };
  auto unchanged=[](const nouveau_horizon_channel& a,const nouveau_horizon_channel& b){
    return a.gpu_channel.num_entries==b.gpu_channel.num_entries&&a.pending_dwords==b.pending_dwords&&
        a.pending_command_bytes==b.pending_command_bytes&&a.cache_acquire_emitted==b.cache_acquire_emitted&&
        a.pending_wait_slices==b.pending_wait_slices&&a.next_wait_slice==b.next_wait_slice;
  };
  nouveau_horizon_exec execs[128]{};
  for(auto& e:execs){e.addr=0x10000;e.size_B=16;}
  for(bool mapped:{false,true})for(bool order:{false,true})for(bool acquired:{false,true}){
    nouveau_horizon_device dev{order,true,false};
    for(unsigned initial=GPFIFO_QUEUE_SIZE-20;initial<=GPFIFO_QUEUE_SIZE;++initial){
      nouveau_horizon_channel base{};base.device=&dev;base.mapped_completion_enabled=mapped;
      base.cache_acquire_emitted=acquired;base.gpu_channel.num_entries=initial;
      if(!flushable(base))continue;
      for(unsigned count=1;count<=128;++count)for(bool incomplete:{false,true}){
        for(unsigned j=0;j<count;++j)execs[j].incomplete=incomplete&&j+1<count;
        auto c=base;const auto status=nouveau_horizon_channel_exec_locked(&c,count,execs);
        if(status==NOUVEAU_HORIZON_SUCCESS)check(flushable(c),"Accepted exec must leave room for submit completion and worst-case total-order wait");
        else check(status==NOUVEAU_HORIZON_ERROR_NO_SPACE&&unchanged(base,c),"Capacity-rejected exec must not accept a prefix");
      }
      auto barrier=base;auto status=nouveau_horizon_channel_full_barrier_locked(&barrier);
      check(status==NOUVEAU_HORIZON_SUCCESS?flushable(barrier):unchanged(base,barrier),"Standalone barrier must preserve completion headroom or reject atomically");
      auto waited=base;const nouveau_horizon_fence fence{1,1};
      status=nouveau_horizon_channel_append_waits_locked(&waited,1,&fence,false);
      check(status==NOUVEAU_HORIZON_SUCCESS?flushable(waited):unchanged(base,waited),"Client wait capacity rejection must not consume commands or a wait slice");
    }
    // Mixed public-producer streams, flushing only on NO_SPACE like NVK.
    nouveau_horizon_channel c{};c.device=&dev;c.mapped_completion_enabled=mapped;
    for(unsigned step=0;step<3000;++step){
      auto before=c;nouveau_horizon_status status;
      if(step%7==0)status=nouveau_horizon_channel_full_barrier_locked(&c);
      else if(step%3==0){const nouveau_horizon_fence fence{1,1};status=nouveau_horizon_channel_append_waits_locked(&c,1,&fence,false);}
      else{execs[0].incomplete=false;status=nouveau_horizon_channel_exec_locked(&c,1,execs);}
      if(status==NOUVEAU_HORIZON_ERROR_NO_SPACE){
        check(unchanged(before,c)&&flushable(c),"Every full mixed batch remains submittable with no lost prefix");
        c={};c.device=&dev;c.mapped_completion_enabled=mapped;
      }else check(status==NOUVEAU_HORIZON_SUCCESS&&flushable(c),"Accepted mixed stream must retain completion headroom");
    }
    // The atomic API preflights all exec/acquire/barrier/tail entries together.
    for(bool barrier:{false,true})for(unsigned count=1;count<=128;++count){
      nouveau_horizon_channel c{};c.device=&dev;c.mapped_completion_enabled=mapped;
      c.gpu_channel.num_entries=GPFIFO_QUEUE_SIZE-140;
      const auto before=c;
      const unsigned acquire=2,tail=mapped?2:1,extra_order=order?1:0;
      auto status=nouveau_horizon_channel_reserve_entries_locked(&c,count+acquire+(barrier?3:0)+tail+extra_order,0);
      if(status==NOUVEAU_HORIZON_SUCCESS){
        for(auto& e:execs)e.incomplete=false;
        check(nouveau_horizon_channel_exec_locked(&c,count,execs)==NOUVEAU_HORIZON_SUCCESS,"Atomic outer reservation must imply exec preflight");
        if(barrier)check(nouveau_horizon_channel_full_barrier_locked(&c)==NOUVEAU_HORIZON_SUCCESS,"Atomic outer reservation must imply barrier preflight after exec");
        check(flushable(c),"Atomic accepted packet remains flushable");
      }else check(unchanged(before,c),"Atomic outer capacity rejection leaves commands untouched");
    }
  }
  std::printf("PASS: %u actual Mesa producer capacity/ordering/atomicity checks\n",checks);
}
