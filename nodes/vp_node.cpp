
#include "vp_node.h"

namespace vp_nodes {
    
    vp_node::vp_node(std::string node_name): node_name(node_name) {
    }
    
    vp_node::~vp_node() {

    }

    // there is only one thread poping data from the in_queue, we don't use lock here when poping.
    // there is only one thread pushing data to the out_queue, we don't use lock here when pushing.
    void vp_node::handle_run() {
        // cache for batch handling if need
        std::vector<std::shared_ptr<vp_objects::vp_frame_meta>> frame_meta_batch_cache;
        while (true) {
            if (this->active) {
                // wait for producer, make sure in_queue is not empty.
                this->in_queue_semaphore.wait();

                std::cout << this->node_name << ", handle meta, before in_queue.size()==>" << this->in_queue.size() << std::endl;
                auto in_meta = this->in_queue.front();
                
                // handling hooker activated if need
                if (this->meta_handling_hooker) {
                    meta_handling_hooker(node_name, in_queue.size(), in_meta);
                }

                std::shared_ptr<vp_objects::vp_meta> out_meta;
                auto batch_complete = false;

                // call handlers
                if (in_meta->meta_type == vp_objects::vp_meta_type::CONTROL) {
                    auto meta_2_handle = std::dynamic_pointer_cast<vp_objects::vp_control_meta>(in_meta);
                    out_meta = this->handle_control_meta(meta_2_handle);
                }
                else if (in_meta->meta_type == vp_objects::vp_meta_type::FRAME) {    
                    auto meta_2_handle = std::dynamic_pointer_cast<vp_objects::vp_frame_meta>(in_meta);
                    // one by one
                    if (frame_meta_handle_batch == 1) {                    
                        out_meta = this->handle_frame_meta(meta_2_handle);
                    } 
                    else {
                        // batch by batch
                        frame_meta_batch_cache.push_back(meta_2_handle);
                        if (frame_meta_batch_cache.size() >= frame_meta_handle_batch) {
                            // cache complete
                            this->handle_frame_meta(frame_meta_batch_cache);
                            batch_complete = true;
                        } 
                        else {
                            // cache not complete, do nothing
                            std::cout << this->node_name << ", handle meta with batch, frame_meta_batch_cache.size()==>" << frame_meta_batch_cache.size() << std::endl;
                        }
                    }
                }
                else {
                    throw "invalid meta type!";
                }
                this->in_queue.pop();
                std::cout << this->node_name << ", handle meta, after in_queue.size()==>" << this->in_queue.size() << std::endl;

                // one by one mode
                // return nullptr means do not push it to next nodes(such as in des nodes).
                if (out_meta != nullptr && node_type() != vp_node_type::DES) {
                    std::cout << this->node_name << ", handle meta, before out_queue.size()==>" << this->out_queue.size() << std::endl;
                    this->out_queue.push(out_meta);

                    // handled hooker activated if need
                    if (this->meta_handled_hooker) {
                        meta_handled_hooker(node_name, out_queue.size(), out_meta);
                    }

                    // notify consumer of out_queue
                    this->out_queue_semaphore.signal();
                    std::cout << this->node_name << ", handle meta, after out_queue.size()==>" << this->out_queue.size() << std::endl;
                }

                // batch by batch mode
                if (batch_complete && node_type() != vp_node_type::DES) {
                    // push to out_queue one by one
                    for (auto& i: frame_meta_batch_cache) {
                        std::cout << this->node_name << ", handle meta, before out_queue.size()==>" << this->out_queue.size() << std::endl;
                        this->out_queue.push(i);

                        // handled hooker activated if need
                        if (this->meta_handled_hooker) {
                            meta_handled_hooker(node_name, out_queue.size(), i);
                        }

                        // notify consumer of out_queue
                        this->out_queue_semaphore.signal();
                        std::cout << this->node_name << ", handle meta, after out_queue.size()==>" << this->out_queue.size() << std::endl;
                    }
                    // clean cache for the next batch
                    frame_meta_batch_cache.clear();
                }
                
            }
            else {
                // sleep for 1 ms
                std::this_thread::sleep_for(std::chrono::microseconds{1000});
            }
        }
    }

    // there is only one thread poping from the out_queue, we don't use lock here when poping.
    void vp_node::dispatch_run() {
        while (true) {
            if(this->active) {
                // wait for producer, make sure out_queue is not empty.
                this->out_queue_semaphore.wait();

                std::cout << this->node_name << ", dispatch meta, before out_queue.size()==>" << this->out_queue.size() << std::endl;
                auto out_meta = this->out_queue.front();

                // leaving hooker activated if need
                if (this->meta_leaving_hooker) {
                    meta_leaving_hooker(node_name, out_queue.size(), out_meta);
                }

                // do something..
                this->push_meta(out_meta);
                this->out_queue.pop();
                std::cout << this->node_name << ", dispatch meta, after out_queue.size()==>" << this->out_queue.size() << std::endl;
            }
            else {
                // sleep for 1 ms
                std::this_thread::sleep_for(std::chrono::microseconds{1000});
            }
        }
    }

    
    std::shared_ptr<vp_objects::vp_meta> vp_node::handle_frame_meta(std::shared_ptr<vp_objects::vp_frame_meta> meta) {
        return meta;
    }

    std::shared_ptr<vp_objects::vp_meta> vp_node::handle_control_meta(std::shared_ptr<vp_objects::vp_control_meta> meta) {
        return meta;
    }

    void vp_node::handle_frame_meta(const std::vector<std::shared_ptr<vp_objects::vp_frame_meta>>& meta_with_batch) {
        
    }

    void vp_node::meta_flow(std::shared_ptr<vp_objects::vp_meta> meta) {
        if (meta == nullptr) {
            return;
        }
               
        if(meta->meta_type == vp_objects::vp_meta_type::CONTROL) {
            auto control_meta = std::dynamic_pointer_cast<vp_objects::vp_control_meta>(meta);
            switch (control_meta->control_type)
            {
            case vp_objects::vp_control_type::START:
                this->active = true;
                break;
            case vp_objects::vp_control_type::STOP:
                this->active = false;
                break;
            default:
                break;
            }
        }
        std::lock_guard<std::mutex> guard(this->in_queue_lock);
        std::cout << this->node_name << ", meta flow, before in_queue.size()==>" << this->in_queue.size() << std::endl;
        this->in_queue.push(meta);

        // arriving hooker activated if need
        if (this->meta_arriving_hooker) {
            meta_arriving_hooker(node_name, in_queue.size(), meta);
        }
        
        // notify consumer of in_queue
        this->in_queue_semaphore.signal();
        std::cout << this->node_name << ", meta flow, after in_queue.size()==>" << this->in_queue.size() << std::endl;
    }

    void vp_node::detach() {
        for(auto i : this->pre_nodes) {
            i->remove_subscriber(shared_from_this());
        }
        this->pre_nodes.clear();
    }

    void vp_node::attach_to(std::vector<std::shared_ptr<vp_node>> pre_nodes) {
        // can not attach src node to any previous nodes
        if (this->node_type() == vp_node_type::SRC) {
            throw vp_excepts::vp_invalid_calling_error("SRC nodes must not have any previous nodes!");
        }
        // can not attach any nodes to des node
        for(auto i : pre_nodes) {
            if (i->node_type() == vp_node_type::DES) {
                throw vp_excepts::vp_invalid_calling_error("DES nodes must not have any next nodes!");
            }
            i->add_subscriber(shared_from_this());
            this->pre_nodes.push_back(i);
        }
    }

    void vp_node::initialized() {
        // start threads since all resources have been initialized
        if (1)
        {
            // TO-DO, check if started already
        }
        
        this->handle_thread = std::thread(&vp_node::handle_run, this);
        this->dispatch_thread = std::thread(&vp_node::dispatch_run, this);
    }

    vp_node_type vp_node::node_type() {
        // return vp_node_type::MID by default
        // need override in child class
        return vp_node_type::MID;
    }

    std::vector<std::shared_ptr<vp_node>> vp_node::next_nodes() {
        std::vector<std::shared_ptr<vp_node>> next_nodes;
        for(auto & i: this->subscribers) {
            next_nodes.push_back(std::dynamic_pointer_cast<vp_node>(i));
        }
        return next_nodes;
    }

    std::string vp_node::to_string() {
        // return node_name by default
        return node_name;
    }
}